#include "file.h"
#include "config.h"
#include "memory/memory.h"
#include "memory/heap/kheap.h"
#include "status.h"
#include "kernel.h"
#include "fat/fat16.h"
#include "disk/disk.h"
#include "string/string.h"

// virtual file system layer

// every file system in the system
struct file_system *file_systems[CARBONOS_MAX_FILE_SYSTEMS];
struct file_descriptor *file_descriptors[CARBONOS_MAX_FILE_DESCRIPTORS];

// get us a free file system slot in file_systems array
static struct file_system **fs_get_free_file_system()
{
    int i = 0;
    for (i = 0; i < CARBONOS_MAX_FILE_SYSTEMS; i++)
    {
        // get empty file system slot and return its addr
        // if file system index is pointing to nothing
        if (file_systems[i] == 0)
        {
            return &file_systems[i];
        }
    }
    return 0;
}

// 
void fs_insert_file_system(struct file_system *file_system)
{
    struct file_system **fs;

    fs = fs_get_free_file_system();
    if (!fs)
    {
        print("Problem inserting file system");
        while (1) {}
    }

    // *fs points to file system inserted addr
    // **fs points to file system data
    *fs = file_system;
}

// loads file system in the file system array
static void fs_static_load()
{
    fs_insert_file_system(fat16_init());
}

// creates all 12 file systems, zeroes them, and loads them
void fs_load()
{
    memset(file_systems, 0, sizeof(file_systems));
    fs_static_load();
}

// creates all 512 file descriptors, zeroes them, and loads them
void fs_init()
{
    memset(file_descriptors, 0, sizeof(file_descriptors));
    fs_load();
}

// finds available index, creates descriptor in memory, adds to descriptor array
// desc_out points to address of pointer of new descriptor
// *desc_out points to new descriptor address
// **desc_out points to new descriptor data
static int file_new_descriptor(struct file_descriptor **desc_out)
{
    int res = -ENOMEM;
    
    for (int i = 0; i < CARBONOS_MAX_FILE_DESCRIPTORS; i++)
    {
        // found file descriptor free to use
        if (file_descriptors[i] == 0)
        {
            struct file_descriptor *desc = kzalloc(sizeof(struct file_descriptor));

            //descriptors start at 1
            desc->index = i + 1;
            file_descriptors[i] = desc;
            *desc_out = desc;
            res = 0;
            break;
        }
    }

    return res;
}

// returns file descriptor relative to the index param
static struct file_descriptor *file_get_descriptor(int fd)
{
    if (fd <= 0 || fd >= CARBONOS_MAX_FILE_DESCRIPTORS)
    {
        return 0;
    }

    // descriptors start at 1
    int index = fd - 1;
    return file_descriptors[index];
}

// binds disk to its respective file system
struct file_system *fs_resolve(struct disk *disk)
{
    struct file_system *fs = 0;
    for (int i = 0; i < CARBONOS_MAX_FILE_SYSTEMS; i++)
    {
        // if file system exists and its designed for this disk
        if (file_systems[i] != 0 && file_systems[i]->resolve(disk) == 0)
        {
            fs = file_systems[i];
            break;
        }
    }

    return fs;
}

// get fopen mode
FILE_MODE file_get_mode_by_string(const char *str)
{
    FILE_MODE mode = FILE_MODE_INVALID;
    if (strncmp(str, "r", 1) == 0)
    {
        mode = FILE_MODE_READ;
    }
    else if (strncmp(str, "w", 1) == 0)
    {
        mode = FILE_MODE_WRITE;
    }
    else if (strncmp(str, "a", 1) == 0)
    {
        mode = FILE_MODE_APPEND;
    }
    return mode;
}

// virtual open file function
int fopen(const char *file_name, const char *mode_str)
{
    int res = 0;

    // gets path parsed
    struct path_root *root_path = pparser_parse(file_name, NULL);
    if (!root_path)
    {
        res = -EINVARG;
        goto out;
    }

    // needs more than just root path 0:/ needs 0:/first_part
    if (!root_path->first)
    {
        res = -EINVARG;
        goto out;
    }

    // ensure disk exists
    struct disk *disk = disk_get(root_path->drive_num);
    if (!disk)
    {
        res = -EIO;
        goto out;
    }

    // check if disk has file system linked
    if (!disk->file_system)
    {
        res = -EIO;
        goto out;
    }

    // get fopen mode
    FILE_MODE mode = file_get_mode_by_string(mode_str);
    if (mode == FILE_MODE_INVALID)
    {
        res = -EINVARG;
        goto out;
    }

    // call non virtual fopen (fat16_open e.g) 
    void *descriptor_private_data = disk->file_system->open(disk, root_path->first, mode);

    if (ISERR(descriptor_private_data))
    {
        res = ERROR_I(descriptor_private_data);
        goto out;
    }

    struct file_descriptor *desc = 0;
    res = file_new_descriptor(&desc);
    if (res < 0)
    {
        goto out;
    }
    desc->file_system = disk->file_system;
    desc->private = descriptor_private_data;
    desc->disk = disk;
    res = desc->index;

out:
    if (res < 0)
        res = 0;

    return res;
}

// nmemb: memory blocks
int fread(void *ptr, uint32_t size, uint32_t nmemb, int fd)
{
    int res = 0;
    if (size == 0 || nmemb == 0 || fd < 1)
    {
        res = -EINVARG;
        goto out;
    }

    struct file_descriptor *desc = file_get_descriptor(fd);
    if (!desc)
    {
        res = -EINVARG;
        goto out;
    }

    res = desc->file_system->read(desc->disk, desc->private, size, nmemb, (char *)ptr);

out:
    return res;
}

// shifts file as defined in offset
int fseek(int fd, int offset, FILE_SEEK_MODE whence)
{
    int res = 0;
    struct file_descriptor *desc = file_get_descriptor(fd);
    if (!desc)
    {
        res = -EIO;
        goto out;
    }

    res = desc->file_system->seek(desc->private, offset, whence);

out:
    return res;
}