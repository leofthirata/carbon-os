#include "fat16.h"
#include "status.h"
#include "string/string.h"
#include "disk/disk.h"
#include "disk/streamer.h"
#include "memory/memory.h"
#include "memory/heap/kheap.h"
#include "kernel.h"

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

struct fat_file_descriptor
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
int fat16_read(struct disk *disk, void *descriptor, uint32_t size, uint32_t nmemb, char *out);
int fat16_seek(void *private, uint32_t offset, FILE_SEEK_MODE seek_mode);
int fat16_stat(struct disk *disk, void *private, struct file_stat *stat);
int fat16_close(void *private);

struct file_system fat16_fs =
{
    .resolve = fat16_resolve,
    .open = fat16_open,
    .read = fat16_read,
    .seek = fat16_seek,
    .stat = fat16_stat,
    .close = fat16_close
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

// translate disk stream bytes to fat16 data and counts how many items and returns
int fat16_get_total_items_for_dir(struct disk *disk, uint32_t dir_start_sector)
{
    struct fat_directory_item item;
    struct fat_directory_item empty_item;
    memset(&empty_item, 0, sizeof(empty_item));

    struct fat_private *fat_private = disk->fs_private;

    int res = 0;
    int dir_start_pos = dir_start_sector * disk->sector_size;
    struct disk_stream *stream = fat_private->directory_stream;

    // move stream index to dir_start_pos
    if (disk_streamer_seek(stream, dir_start_pos) != CARBONOS_OK)
    {
        res = -EIO;
        goto out;
    }

    int i = 0;
    // counts how many items in directory
    while (1)
    {
        // translate bytes to fat16 data 
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

    // FirstRootDirSecNum = BPB_ResvdSecCnt + (BPB_NumFATs * BPB_FATSz16) 
    // https://academy.cba.mit.edu/classes/networking_communications/SD/FAT.pdf
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

// strip spaces
// if inserted "FILE    "
// result will be "FILE\0"
void fat16_to_proper_str(char **out, const char *in)
{
    while (*in != 0x00 && *in != 0x20)
    {
        **out = *in;
        *out += 1;
        in++;
    }

    if (*in == 0x20)
    {
        **out = 0x00;
    }
}

// strips space in file name and add file extension
void fat16_get_full_relative_filename(struct fat_directory_item *item, char *out, int max_len)
{
    memset(out, 0x00, max_len);
    char *out_tmp = out;
    fat16_to_proper_str(&out_tmp, (const char *) item->filename);
    if (item->ext[0] != 0x00 && item->ext[0] != 0x20)
    {
        *out_tmp++ = '.';
        fat16_to_proper_str(&out_tmp, (const char *) item->ext);
    }
}

struct fat_directory_item *fat16_clone_dir_item(struct fat_directory_item *item, int size)
{
    struct fat_directory_item *item_copy = 0;
    if (size < sizeof(struct fat_directory_item))
    {
        return 0;
    }

    item_copy = kzalloc(size);
    if (!item_copy)
    {
        return 0;
    }

    memcpy(item_copy, item, size);
    return item_copy;
}

static uint32_t fat16_get_first_cluster(struct fat_directory_item *item)
{
    return (item->high_16_bits_first_cluster) | item->low_16_bits_first_cluster; 
}

// converts cluster number to sector
// FirstSectorofCluster = ((N – 2) * BPB_SecPerClus) + FirstDataSector;
// https://academy.cba.mit.edu/classes/networking_communications/SD/FAT.pdf
static int fat16_cluster_to_sector(struct fat_private *private, int cluster)
{
    return private->root_directory.ending_sector_pos + ((cluster - 2) * private->header.primary_header.sectors_per_cluster);
}

// file allocation tables comes after reserved sectors
static uint32_t fat16_get_first_fat_sector(struct fat_private *private)
{
    return private->header.primary_header.reserved_sectors;
}

static int fat16_get_fat_entry(struct disk *disk, int cluster)
{
    int res = -1;
    struct fat_private *private = disk->fs_private;
    struct disk_stream *stream = private->fat_read_stream;
    if (!stream)
    {
        goto out;
    }

    uint32_t fat_table_position = fat16_get_first_fat_sector(private) * disk->sector_size;
    res = disk_streamer_seek(stream, fat_table_position * (cluster * CARBONOS_FAT16_FAT_ENTRY_SIZE));
    if (res < 0)
    {
        goto out;
    }

    uint16_t result = 0;
    res = disk_streamer_read(stream, &result, sizeof(result));
    if (res < 0)
    {
        goto out;
    }

    res = result;

out: 
    return res;
}

// get correct cluster to use based on the start cluster and offset
static int fat16_get_cluster_for_offset(struct disk *disk, int start_cluster, int offset)
{
    int res = 0;
    struct fat_private *private = disk->fs_private;
    int size_of_cluster_bytes = private->header.primary_header.sectors_per_cluster * disk->sector_size;
    int cluster_to_use = start_cluster;
    int clusters_ahead = offset / size_of_cluster_bytes;
    for (int i = 0; i < clusters_ahead; i++)
    {
        int entry = fat16_get_fat_entry(disk, cluster_to_use);
        // check if bad flag or end flag
        if (entry == 0xFF8 || entry == 0xFFF)
        {
            // last file entry
            res = -EIO;
            goto out;
        }

        // sector is bad
        if (entry == CARBONOS_FAT16_BAD_SECTOR)
        {
            res = -EIO;
            goto out;
        }

        // reserved sector
        if (entry == 0xFF0 || entry == 0xFF6)
        {
            res = -EIO;
            goto out;
        }

        // no cluster
        if (entry == 0x00)
        {
            res = -EIO;
            goto out;
        }

        cluster_to_use = entry;
    }

    res = cluster_to_use;
out:
    return res;
}

// use streamer to read disk data according to offset
static int fat16_read_internal_from_stream(struct disk *disk, struct disk_stream *stream, int cluster, int offset, int total, void *out)
{
    int res = 0;
    struct fat_private *private = disk->fs_private;
    int size_of_cluster_bytes = private->header.primary_header.sectors_per_cluster * disk->sector_size;
    int cluster_to_use = fat16_get_cluster_for_offset(disk, cluster, offset);
    if (cluster_to_use < 0)
    {
        res = cluster_to_use;
        goto out;
    }

    int offset_from_cluster = offset % size_of_cluster_bytes;
    int start_sector = fat16_cluster_to_sector(private, cluster_to_use);
    int start_pos = (start_sector * disk->sector_size) + offset_from_cluster;
    int total_to_read = total > size_of_cluster_bytes ? size_of_cluster_bytes : total;
    res = disk_streamer_seek(stream, start_pos);
    if (res != CARBONOS_OK)
    {
        goto out;
    }

    // buffers stream data to out buf
    res = disk_streamer_read(stream, out, total_to_read);
    if (res != CARBONOS_OK)
    {
        goto out;
    } 

    // if there are more data to read, recall function
    total -= total_to_read;
    if (total > 0)
    {
        res = fat16_read_internal_from_stream(disk, stream, cluster, offset + total_to_read, total, out + total_to_read);
    }
out:
    return res;
}

// use stream disk to read data
static int fat16_read_internal(struct disk *disk, int start_cluster, int offset, int total, void *out)
{
    struct fat_private *fs_private = disk->fs_private;
    struct disk_stream *stream = fs_private->cluster_read_stream;
    return fat16_read_internal_from_stream(disk, stream, start_cluster, offset, total, out);
}

void fat16_free_dir(struct fat_directory *dir)
{
    if (!dir)
    {
        return;
    }

    if (dir->item)
    {
        kfree(dir->item);
    }

    kfree(dir);
}

void fat16_fat_item_free(struct fat_item *item)
{
    if (item->type == FAT_ITEM_TYPE_DIRECTORY)
    {
        fat16_free_dir(item->directory);
    }
    else if (item->type == FAT_ITEM_TYPE_FILE)
    {
        kfree(item->item);
    }

    kfree(item);
}


struct fat_directory *fat16_load_fat_dir(struct disk *disk, struct fat_directory_item *item)
{
    int res = 0;
    struct fat_directory *directory = 0;
    struct fat_private *fat_private = disk->fs_private;
    // if its not a directory
    if (!(item->attribute & FAT_FILE_SUBDIRECTORY))
    {
        res = -EINVARG;
        goto out;
    }

    directory = kzalloc(sizeof(struct fat_directory));
    if (!directory)
    {
        res = -ENOMEM;
        goto out;
    }

    // get first cluster index
    // cluster is like the sector for the fat16 file system
    int cluster = fat16_get_first_cluster(item);
    // converts its cluster index to sector 
    int cluster_sector = fat16_cluster_to_sector(fat_private, cluster);
    // total items in dir
    int total_items = fat16_get_total_items_for_dir(disk, cluster_sector);
    directory->total = total_items;
    int directory_size = directory->total * sizeof(struct fat_directory_item);
    directory->item = kzalloc(directory_size);
    if (!directory->item)
    {
        res = -ENOMEM;
        goto out;
    }

    // stores cluster data in offset 0x00 to item
    res = fat16_read_internal(disk, cluster, 0x00, directory_size, directory->item);
    if (res != CARBONOS_OK)
    {
        goto out;
    }

out:
    if (res != CARBONOS_OK)
    {
        fat16_free_dir(directory);
    }

    return directory;
}

// allocates memory to new item
// creates new item/dir in fat16
struct fat_item *fat16_new_fat_item_for_dir_item(struct disk *disk, struct fat_directory_item *item)
{
    struct fat_item *f_item = kzalloc(sizeof(struct fat_item));
    if (!f_item)
    {
        return 0;
    }

    // if path is for a directory
    if (item->attribute & FAT_FILE_SUBDIRECTORY)
    {
        // loads data to directory
        f_item->directory = fat16_load_fat_dir(disk, item);
        f_item->type = FAT_ITEM_TYPE_DIRECTORY;
    }

    // if path is for a file
    f_item->type = FAT_ITEM_TYPE_FILE;
    f_item->item = fat16_clone_dir_item(item, sizeof(struct fat_directory_item));
    return f_item;
}

// find item specified by its name/path
struct fat_item *fat16_find_item_in_dir(struct disk *disk, struct fat_directory *directory, const char *name)
{
    struct fat_item *f_item = 0;
    char tmp_filename[CARBONOS_PATH_MAX_SIZE];
    for (int i = 0; i < directory->total; i++)
    {
        // get filename and file extension and merge
        // test + .txt
        fat16_get_full_relative_filename(&directory->item[i], tmp_filename, sizeof(tmp_filename));
        if (istrncmp(tmp_filename, name, sizeof(tmp_filename)) == 0)
        {
            // item found, create new fat item
            f_item = fat16_new_fat_item_for_dir_item(disk, &directory->item[i]);
        }
    }

    return f_item;
}

struct fat_item *fat16_get_dir_entry(struct disk *disk, struct path_part *path)
{
    struct fat_private *fat_private = disk->fs_private;
    struct fat_item *current_item = 0;
    // if path is 0:/dir/item.txt, root item will be dir
    struct fat_item *root_item = fat16_find_item_in_dir(disk, &fat_private->root_directory, path->part);
    if (!root_item)
    {
        goto out;
    }

    // if there are more items in the path to read
    struct path_part *next_part = path->next;
    current_item = root_item;
    while (next_part != 0)
    {
        if (current_item->type != FAT_ITEM_TYPE_DIRECTORY)
        {
            current_item = 0;
            break;
        }

        // get the file and free the past items
        struct fat_item *tmp_item = fat16_find_item_in_dir(disk, current_item->directory, next_part->part);
        fat16_fat_item_free(current_item);
        current_item = tmp_item;
        next_part = next_part->next;
    }
out:
    return current_item;
}


// open path and returns pointer to either directory or file
void *fat16_open(struct disk *disk, struct path_part *path, FILE_MODE mode)
{
    if (mode != FILE_MODE_READ)
    {
        return ERROR(-ERDONLY);
    }

    // creates item/directory
    struct fat_file_descriptor *descriptor = 0;
    descriptor = kzalloc(sizeof(struct fat_file_descriptor));
    if (!descriptor)
    {
        return ERROR(-ENOMEM);
    }

    // get file for given path          
    descriptor->item = fat16_get_dir_entry(disk, path);
    if (!descriptor->item)
    {
        return ERROR(-EIO);
    }

    descriptor->pos = 0;
    return descriptor;
}

int fat16_read(struct disk *disk, void *descriptor, uint32_t size, uint32_t nmemb, char *out)
{
    int res = 0;
    struct fat_file_descriptor *fat_desc = descriptor;
    struct fat_directory_item *item = fat_desc->item->item;
    int offset = fat_desc->pos;
    for (uint32_t i = 0; i < nmemb; i++)
    {
        res = fat16_read_internal(disk, fat16_get_first_cluster(item), offset, size, out);
        if (ISERR(res))
        {
            goto out;
        }

        out += size;
        offset += size;
    }

    res = nmemb;
out:
    return res;
}

// searches for item type file and shifts current position according to mode
int fat16_seek(void *private, uint32_t offset, FILE_SEEK_MODE seek_mode)
{
    int res = 0;

    struct fat_file_descriptor *desc = private;
    struct fat_item *desc_item = desc->item;
    if (desc_item->type != FAT_ITEM_TYPE_FILE)
    {
        res = -EINVARG;
        goto out;
    }

    struct fat_directory_item *ritem = desc_item->item;
    if (offset >= ritem->filesize)
    {
        res = -EIO;
        goto out;
    }

    switch (seek_mode)
    {
        case SEEK_SET:
        {
            desc->pos = offset;
            break;
        }
        case SEEK_END:
        {
            res = -EUNIMP;
            break;
        }
        case SEEK_CUR:
        {
            desc->pos += offset;
            break;
        }
        default:
        {
            res = -EINVARG;
            break;
        }
    }

out:
    return res;
}

int fat16_stat(struct disk *disk, void *private, struct file_stat *stat)
{
    int res = 0;
    struct fat_file_descriptor *descriptor = (struct fat_file_descriptor *)private;
    struct fat_item *desc_item = descriptor->item;
    if (desc_item->type != FAT_ITEM_TYPE_FILE)
    {
        res = -EINVARG;
        goto out;
    }

    struct fat_directory_item *ritem = desc_item->item;
    stat->size = ritem->filesize;
    stat->flags = 0x00;

    if (ritem->attribute & FAT_FILE_READ_ONLY)
    {
        stat->flags |= FILE_STAT_READ_ONLY;
    }

out:
    return res;
}

static void fat16_free_file_descriptor(struct fat_file_descriptor *descriptor)
{
    fat16_fat_item_free(descriptor->item);
    kfree(descriptor);
}

int fat16_close(void *private)
{
    fat16_free_file_descriptor((struct fat_file_descriptor *)private);
    return 0;
}