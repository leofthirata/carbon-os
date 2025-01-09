#ifndef STREAMER_H
#define STREAMER_H

#include "disk.h"

struct disk_stream
{
    // byte position
    int pos;
    struct disk* disk;
};

struct disk_stream *disk_streamer_new(int disk_id);
int disk_streamer_read(struct disk_stream *stream, void *out, int total);
int disk_streamer_seek(struct disk_stream *stream, int pos);
void disk_streamer_close(struct disk_stream *stream);

#endif