#ifndef FILE_H
#define FILE_H

#include "pparser.h"
#include <stdint.h>

typedef unsigned int FILE_SEEK_MODE;
enum
{
    SEEK_SET,
    SEEK_CUR,
    SEEK_EN,
};

typedef unsigned int FILE_MODE;
enum
{
    FILE_MODE_READ,
    FILE_MODE_WRITE,
    FILE_MODE_APPEND,
    FILE_MODE_INVALID,
};

struct disk;

typedef int (*FS_RESOLVE_FUNCTION)(struct disk *disk);
typedef int (*FS_READ_FUNCTION)(struct disk *disk, void *private, uint32_t size, uint32_t nmemb, char *out);
typedef void *(*FS_OPEN_FUNCTION)(struct disk *disk, struct path_part *path, FILE_MODE mode);

struct file_system
{
    // filesystem return zero from resolve if disk is using its filesystem
    FS_RESOLVE_FUNCTION resolve;
    FS_OPEN_FUNCTION open;
    FS_READ_FUNCTION read;

    // fielsystem name
    char name[20];
};

struct file_descriptor
{
    // descriptor index
    int index;
    struct file_system *file_system;

    // private data
    void *private;

    // disk file descriptor should be used with
    struct disk *disk;
};

void fs_init();
int fopen(const char *file_name, const char *mode_str);
int fread(void *ptr, uint32_t size, uint32_t nmemb, int fd);
void fs_insert_filesystem(struct file_system *file_system);
struct file_system *fs_resolve(struct disk *disk);

#endif