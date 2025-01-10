#include "file.h"
#include "config.h"
#include "memory/memory.h"
#include "memory/heap/kheap.h"
#include "status.h"
#include "kernel.h"
#include "fat/fat16.h"

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
        if (file_systems[i] == 0)
        {
            return &file_systems[i];
        }
    }
    return 0;
}

void fs_insert_file_system(struct file_system *file_system)
{
    struct file_system **fs;
    fs = fs_get_free_file_system();
    if (!fs)
    {
        print("Problem inserting file system");
        while (1) {}
    }

    *fs = file_system;
}

static void fs_static_load()
{
    fs_insert_file_system(fat16_init());
}

void fs_load()
{
    memset(file_systems, 0, sizeof(file_systems));
    fs_static_load();
}

void fs_init()
{
    memset(file_descriptors, 0, sizeof(file_descriptors));
    fs_load();
}

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

struct file_system *fs_resolve(struct disk *disk)
{
    struct file_system *fs = 0;
    for (int i = 0; i < CARBONOS_MAX_FILE_SYSTEMS; i++)
    {
        if (file_systems[i] != 0 && file_systems[i]->resolve(disk) == 0)
        {
            fs = file_systems[i];
            break;
        }
    }

    return fs;
}

int fopen(const char *file_name, const char *mode)
{
    return -EIO;
}