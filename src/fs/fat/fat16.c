#include "fat16.h"
#include "status.h"
#include "string/string.h"
#include "disk/disk.h"
#include "disk/streamer.h"
#include "memory/memory.h"
#include "memory/heap/kheap.h"
#include <stdint.h>

// https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system
// FAT16 signature must be 0x28 or 0x29
#define CARBONOS_FAT16_SIGNATURE 0x29
#define CARBONOS_FAT16_FAT_ENTRY_SIZE 0x02
// If "table_value" equals (==) 0xFFF7 then this cluster has been marked as "bad". "Bad" clusters are prone to errors and should be avoided. If "table_value" is not one of the above cases then it is the cluster number of the next cluster in the file.
#define CARBONOS_FAT16_BAD_SECTOR 0xFF7
#define CARBONOS_FAT16_UNUSED 0x00

typedef unsigned int FAT_ITEM_TYPE;
#define FAT_ITEM_TYPE_DIRECTORY 0
#define FAT_ITEM_TYPE_FILE 1

// fat directory entry attributes bitmask
#define FAT_FILE_READ_ONLY          0x01
#define FAT_FILE_HIDDEN             0x02
#define FAT_FILE_SYSTEM             0x04
#define FAT_FILE_VOLUME_LABEL       0x08
#define FAT_FILE_SUBDIRECTORY       0x10
#define FAT_FILE_ARCHIVE            0x20
#define FAT_FILE_DEVICE             0x40
#define FAT_FILE_RESERVED           0x80

// structs below are to compare and identify with boot.asm if its a fat16
struct fat_header_extended
{
    uint8_t drive_number;
    uint8_t win_nt_bit;
    uint8_t signature;
    uint32_t volume_id;
    uint8_t volume_id_string[11];
    uint8_t system_id_string[8];
} __attribute__((packed));

struct fat_header
{
    uint8_t boot_short_jmp[3];
    uint8_t oem_identifier[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_copies;
    uint16_t root_dir_entries;
    uint16_t number_of_sectors;
    uint8_t media_type;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t number_of_heads;
    uint32_t hidden_sectors;
    uint32_t sectors_big;

    // uint8_t extended_section[54];
} __attribute__((packed));

struct fat_h
{
    struct fat_header primary_header;
    union fat_h_e
    {
        struct fat_header_extended extended_header;
    } shared;
};

// bitmask 
// https://wiki.osdev.org/FAT Standard 8.3 format
struct fat_directory_item
{
    // offset 0 len 11 filename + extension
    uint8_t filename[8];
    uint8_t ext[3];
    // READ_ONLY=0x01 HIDDEN=0x02 SYSTEM=0x04 VOLUME_ID=0x08 DIRECTORY=0x10 ARCHIVE=0x20 LFN=READ_ONLY|HIDDEN|SYSTEM|VOLUME_ID 
    uint8_t attribute;
    // windows nt reserved
    uint8_t reserved;
    // Creation time in hundredths of a second, although the official FAT Specification from Microsoft says it is tenths of a second. Range 0-199 inclusive. Based on simple tests, Ubuntu16.10 stores either 0 or 100 while Windows7 stores 0-199 in this field
    uint8_t creation_time_tenths_of_a_sec;
    // The time that the file was created. Multiply Seconds by 2.
    uint16_t creation_time;
    // The date on which the file was created.
    uint16_t creation_date;
    // Last accessed date. Same format as the creation date.
    uint16_t last_access;
    // The high 16 bits of this entry's first cluster number. For FAT 12 and FAT 16 this is always zero.
    uint16_t high_16_bits_first_cluster;
    // Last modification time. Same format as the creation time.
    uint16_t last_mod_time;
    // Last modification date. Same format as the creation date.
    uint16_t last_mod_date;
    // The low 16 bits of this entry's first cluster number. Use this number to find the first cluster for this entry.
    uint16_t low_16_bits_first_cluster;
    // The size of the file in bytes.
    uint32_t filesize;
} __attribute__((packed));

struct fat_directory
{
    struct fat_directory_item *item;
    int total;
    int sector_pos;
    int ending_sector_pos;
};

struct fat_item
{
    // variables which share same memory
    // its either an item or an directory
    union
    {
        struct fat_directory_item *item;
        struct fat_directory *directory;
    };
    FAT_ITEM_TYPE type;
};

struct fat_item_descriptor
{
    struct fat_item *item;
    uint32_t pos;
};

struct fat_private
{
    struct fat_h header;
    struct fat_directory root_directory;

    // used to stream data clusters
    struct disk_stream *cluster_read_stream;
    // used to stream the file allocation table
    struct disk_stream *fat_read_stream;
    // used to stream the directory
    struct disk_stream *directory_stream;
};

int fat16_resolve(struct disk *disk);
void *fat16_open(struct disk *disk, struct path_part *path, FILE_MODE mode);

struct file_system fat16_fs =
{
    .resolve = fat16_resolve,
    .open = fat16_open
};

struct file_system *fat16_init()
{
    strcpy(fat16_fs.name, "FAT16");
    return &fat16_fs;
}

static void fat16_init_private(struct disk *disk, struct fat_private *private)
{
    memset(private, 0, sizeof(struct fat_private));
    private->cluster_read_stream = disk_streamer_new(disk->id);
    private->fat_read_stream = disk_streamer_new(disk->id);
    private->directory_stream = disk_streamer_new(disk->id);
}

// converts sector number to absolute position (in sector/bytes)
int fat16_sector_to_absolute(struct disk *disk, int sector)
{
    return sector * disk->sector_size;
}

int fat16_get_total_items_for_dir(struct disk *disk, uint32_t dir_start_sector)
{
    struct fat_directory_item item;
    struct fat_directory_item empty_item;
    memset(&empty_item, 0, sizeof(empty_item));

    struct fat_private *fat_private = disk->fs_private;

    int res = 0;
    int dir_start_pos = dir_start_sector * disk->sector_size;
    struct disk_stream *stream = fat_private->directory_stream;

    if (disk_streamer_seek(stream, dir_start_pos) != CARBONOS_OK)
    {
        res = -EIO;
        goto out;
    }

    int i = 0;
    while (1)
    {
        if (disk_streamer_read(stream, &item, sizeof(item)) != CARBONOS_OK)
        {
            res = -EIO;
            goto out;
        }

        if (item.filename[0] == 0x00)
        {
            // done
            break;
        }

        // available dir item
        if (item.filename[0] == 0xE5)
        {
            continue;
        }

        i++;
    }

    res = i;

out:
    return res;
}

int fat16_get_root_dir(struct disk *disk, struct fat_private *fat_private, struct fat_directory *directory)
{
    int res = 0;
    struct fat_header *primary_header = &fat_private->header.primary_header;
    int root_dir_sector_pos = (primary_header->fat_copies * primary_header->sectors_per_fat) + primary_header->reserved_sectors;
    int root_dir_entries = fat_private->header.primary_header.root_dir_entries;
    int root_dir_size = (root_dir_entries * sizeof(struct fat_directory_item));
    int total_sectors = root_dir_size / disk->sector_size;

    // rounds up the amount of sectors depending on root dir size
    if (root_dir_size % disk->sector_size)
        total_sectors++;

    int total_items = fat16_get_total_items_for_dir(disk, root_dir_sector_pos);

    struct fat_directory_item *dir = kzalloc(root_dir_size);
    if (!dir)
    {
        res = -ENOMEM;
        goto out;
    }

    struct disk_stream *stream = fat_private->directory_stream;
    if (disk_streamer_seek(stream, fat16_sector_to_absolute(disk, root_dir_sector_pos)) != CARBONOS_OK)
    {
        res = -EIO;
        goto out;
    }

    if (disk_streamer_read(stream, dir, root_dir_size) != CARBONOS_OK)
    {
        res = -EIO;
        goto out;
    }

    directory->item = dir;
    directory->total = total_items;
    directory->sector_pos = root_dir_sector_pos;
    directory->ending_sector_pos = root_dir_sector_pos + (root_dir_size / disk->sector_size);

out:
    return res;
}

// if return 0, file system is binded to disk
int fat16_resolve(struct disk *disk)
{
    int res = 0;
    struct fat_private *fat_private = kzalloc(sizeof(struct fat_private));
    
    fat16_init_private(disk, fat_private);

    disk->fs_private = fat_private;
    disk->file_system = &fat16_fs;
    
    struct disk_stream *stream = disk_streamer_new(disk->id);
    if (!stream)
    {
        res = -ENOMEM;
        goto out;
    }

    // read fat header
    if (disk_streamer_read(stream, &fat_private->header, sizeof(fat_private->header)) != CARBONOS_OK)
    {
        res = -EIO;
        goto out;
    }

    // header loaded has wrong signature and its not our file system
    if (fat_private->header.shared.extended_header.signature != 0x29)
    {
        res = -EFSNOTUS;
        goto out;
    }

    // if fails to load root dir, returns IO error
    if (fat16_get_root_dir(disk, fat_private, &fat_private->root_directory) != CARBONOS_OK)
    {
        res = -EIO;
        goto out;
    }

out:
    if (stream)
    {
        disk_streamer_close(stream);
    }

    if (res < 0)
    {
        kfree(fat_private);
        disk->fs_private = 0;
    }
    return res;
}

void *fat16_open(struct disk *disk, struct path_part *path, FILE_MODE mode)
{
    return 0;
}