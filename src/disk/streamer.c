#include "streamer.h"
#include "memory/heap/kheap.h"
#include "config.h"

// creates first disk stream which position is 0
// allows us to bind to a disk and read desired bytes
struct disk_stream *disk_streamer_new(int disk_id)
{
    struct disk *disk = disk_get(disk_id);
    if (!disk)
    {
        return 0;
    }

    struct disk_stream *streamer = kzalloc(sizeof(struct disk_stream));
    streamer->pos = 0;
    streamer->disk = disk;
    return streamer;
}

// moves stream position to desired address
int disk_streamer_seek(struct disk_stream *stream, int pos)
{
    stream->pos = pos;

    return 0;
}

// read amount of desired bytes and shifts its index
int disk_streamer_read(struct disk_stream *stream, void *out, int total)
{
    int sector = stream->pos / CARBONOS_SECTOR_SIZE;
    int offset = stream->pos % CARBONOS_SECTOR_SIZE;
    char buf[CARBONOS_SECTOR_SIZE];

    // loads all sector data into buf
    int res = disk_read_block(stream->disk, sector, 1, buf);
    if (res < 0)
    {
        goto out;
    }

    int total_to_read = total > CARBONOS_SECTOR_SIZE ? CARBONOS_SECTOR_SIZE : total;
    
    // increments size of out buf to avoid overflow
    // stores 512 bytes or less in out buf
    for (int i = 0; i < total_to_read; i++)
    {
        *(char *)out++ = buf[offset + i];
    }

    // adjust the stream pos
    stream->pos += total_to_read;

    // in case user wants to read more than 1 sector
    if (total > CARBONOS_SECTOR_SIZE)
    {
        // out buf is already pointing to another sector
        res = disk_streamer_read(stream, out, total - CARBONOS_SECTOR_SIZE);
    }

out:
    return res;
}

void disk_streamer_close(struct disk_stream *stream)
{
    kfree(stream);
}