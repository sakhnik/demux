#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

#include "writer.h"
#include "protocol.h"
#include "message.h"

static void *writer_task(void *arg)
{
    Writer *writer = (Writer *)arg;
    while (!writer->exit_flag)
    {
        usleep(10000);
    }
    pthread_exit(NULL);
}

Writer *writer_init(char const *fname)
{
    int res;

    Writer *writer = malloc(sizeof(Writer));
    if (!writer)
    {
        fprintf(stderr, "Can't alloc memory\n");
        return NULL;
    }

    memset(writer, 0, sizeof(Writer));

    writer->file = fopen(fname, "wb");
    if (!writer->file)
    {
        fprintf(stderr, "Can't open file for writing\n");
        free(writer);
        return NULL;
    }

    res = pthread_create(&writer->thread, NULL, writer_task, writer);
    if (res)
    {
        perror("pthread_create");
        fclose(writer->file);
        free(writer);
        return NULL;
    }

    return writer;
}

static void writer_close_impl(Writer *writer, int give_up)
{
    void *res;

    if (give_up)
    {
        writer->exit_flag = 1;
        __sync_synchronize();
    }
    pthread_join(writer->thread, &res);

    fclose(writer->file);
    free(writer);
}

void writer_close(Writer *writer)
{
    writer_close_impl(writer, 1);
}

void writer_wait_close(Writer *writer)
{
    writer_close_impl(writer, 0);
}

int writer_post(Writer *writer, struct Message *message)
{
    printf("%d\n", protocol_packet_get_size(message));
    message_free(message);
    return 0;
}
