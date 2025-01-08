#include "pparser.h"
#include "kernel.h"
#include "string/string.h"
#include "memory/heap/kheap.h"
#include "memory/memory.h"
#include "status.h"

// check if path is valid
// valid disks: 0:/ 1:/ 6:/
static int pparser_path_valid_format(const char *filename)
{
    int len = strnlen(filename, CARBONOS_PATH_MAX_SIZE);

    return (len >= 3 && is_digit(filename[0]) && memcmp((void *)&filename[1], ":/", 2) == 0);
}

static int pparser_get_drive_by_path(const char **path)
{
    if (!pparser_path_valid_format(*path))
    {
        return -EBADPATH;
    }

    int drive_num = char_to_digit(*path[0]);

    // add 3 bytes to skip drive number 0:/test/ -> /test/
    *path += 3;

    return drive_num;
}

static struct path_root *pparser_create_root(int drive_num)
{
    struct path_root *path_r = kzalloc(sizeof(struct path_root));
    path_r->drive_num = drive_num;
    path_r->first = 0;
    return path_r;
}

// if input 0:/bin/test.bin
// output will be bin
static const char *pparser_path_get_part(const char **path)
{
    char *part = kzalloc(CARBONOS_PATH_MAX_SIZE);
    int i = 0;

    // *path is the address of the pointer which points to the path string
    // so **path points to the path string element
    while (**path != '/' && **path != 0x00)
    {
        part[i] = **path;
        *path += 1;
        i++;
    }

    if (**path == '/')
    {
        *path += 1;
    }

    if (i == 0)
    {
        kfree(part);
        part = 0;
    }

    return part;
}

// create part and return a linked list
struct path_part *pparser_path_parse_part(struct path_part *last, const char **path)
{
    const char *part_str = pparser_path_get_part(path);
    if (!part_str)
    {
        return 0;
    }

    struct path_part *part = kzalloc(sizeof(struct path_part));
    part->part = part_str;
    part->next = 0;

    if (last)
    {
        last->next = part;
    }

    return part;
}

// iterates through everything and free
void pparser_free(struct path_root *root)
{
    struct path_part *part = root->first;
    while (part)
    {
        // save the next item in linked list
        struct path_part *next_part = part->next;
        kfree((void *)part->part);
        kfree(part);
        // restores saved next item in linked list above
        part = next_part;
    }

    kfree(root);
}

// parse path directory into parts in a linked list
// 0:/bin/test.txt -> 0: -> bin -> test.txt
struct path_root *pparser_parse(const char *path, const char *current_dir_path)
{
    int res = 0;
    const char *temp_path = path;
    struct path_root *path_root = 0;

    if (strlen(path) > CARBONOS_PATH_MAX_SIZE)
        goto out;

    res = pparser_get_drive_by_path(&temp_path);
    if (res < 0)
        goto out;

    path_root = pparser_create_root(res);
    if (!path_root)
        goto out;

    struct path_part *first_part = pparser_path_parse_part(NULL, &temp_path);
    if (!first_part)
        goto out;

    // append bin as first part in the linked list of root path
    path_root->first = first_part;

    // if there are more parts, continue appending to the linked list
    struct path_part *part = pparser_path_parse_part(first_part, &temp_path);
    while (part)
    {
        part = pparser_path_parse_part(part, &temp_path);
    }

out:
    return path_root;
}