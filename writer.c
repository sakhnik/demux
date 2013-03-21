#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <sys/socket.h>
#include <fcntl.h>

#include "writer.h"
#include "protocol.h"
#include "message.h"

enum
{
    _READER = 0,
    _WRITER
};

static int do_write(int fd, uint8_t const *data, size_t size)
{
    while (size)
    {
        int res = write(fd, data, size);
        if (res <= 0)
            return -1;
        size -= res;
        data += res;
    }
    return 0;
}

static void *writer_task(void *arg)
{
    Writer *writer = (Writer *)arg;
    while (1)
    {
        struct Message *message = NULL;
        int res = recv(writer->queue[_READER], &message, sizeof(message), 0);
        if (-1 == res)
        {
            perror("recv");
            continue;
        }
        if (!res)
            break;

        if (-1 == do_write(writer->outfd,
                           protocol_packet_get_data(message),
                           protocol_packet_get_size(message)))
        {
            perror("write");
            pthread_exit((void *)-1);
        }
        message_free(message);
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

    if (socketpair(PF_LOCAL, SOCK_SEQPACKET, 0, writer->queue))
    {
        perror("socketpair");
        free(writer);
        return NULL;
    }

    writer->outfd = open(fname, O_WRONLY|O_CREAT|O_TRUNC, 0640);
    if (-1 == writer->outfd)
    {
        fprintf(stderr, "Can't open file for writing\n");
        close(writer->queue[_READER]);
        close(writer->queue[_WRITER]);
        free(writer);
        return NULL;
    }

    res = pthread_create(&writer->thread, NULL, writer_task, writer);
    if (res)
    {
        perror("pthread_create");
        close(writer->queue[_READER]);
        close(writer->queue[_WRITER]);
        close(writer->outfd);
        free(writer);
        return NULL;
    }

    return writer;
}

static void writer_close_impl(Writer *writer, int give_up)
{
    void *res;

    close(writer->queue[_WRITER]);
    pthread_join(writer->thread, &res);

    close(writer->queue[_READER]);
    close(writer->outfd);
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
    int res = send(writer->queue[_WRITER], &message, sizeof(message), 0);
    if (-1 == res)
    {
        perror("send");
        message_free(message);
        return -1;
    }
    return 0;
}
