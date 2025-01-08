#ifndef PPARSER_H
#define PPARSER_H

// e.g. 0:/test/file.bin
// drive_num will be 0:
// first->next will be test
// first->next->next will be file.bin
struct path_root
{
    int drive_num;
    struct path_part *first;
};

// linked list
struct path_part
{
    const char *part; // name of the part
    struct path_part *next;
};

struct path_root *pparser_parse(const char *path, const char *current_dir_path);
void pparser_free(struct path_root *root);

#endif