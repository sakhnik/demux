#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>

#include "message.h"

struct Message *message_alloc(int max_size)
{
    struct Message *message;

    message = (struct Message *) malloc(sizeof(struct Message));
    if (!message)
        return NULL;

    message->data = malloc(max_size);
    if (!message->data)
    {
        free(message);
        return NULL;
    }
    message->max_size = message->size = max_size;

    return message;
}

void message_free(struct Message *message)
{
    free(message->data);
    free(message);
}

int message_send_raw(int size, uint8_t const *data, int sock)
{
    int i, res, total_size = 0;
    uint8_t nsize[] =
    {
        size & 0xFF,
        (size >> 8) & 0xFF,
        (size >> 16) & 0xFF,
        (size >> 24) & 0xFF
    };

    struct iovec iov[] =
    {
        { .iov_base = nsize,            .iov_len = sizeof(nsize) },
        { .iov_base = (void *)data,     .iov_len = size          }
    };

    struct msghdr mh = { 0 };
    mh.msg_iov = iov;
    mh.msg_iovlen = sizeof(iov) / sizeof(iov[0]);

    for (i = 0; i != mh.msg_iovlen; ++i)
        total_size += mh.msg_iov[i].iov_len;

    res = sendmsg(sock, &mh, 0);
    if (!res)
        return 0;
    if (total_size != res)
        return -1;

    return size;
}

int message_send(struct Message const *message, int sock)
{
    return message_send_raw(message->size, message->data, sock);
}

typedef int (*ReallocFunT)(int *max_size, uint8_t **data, int new_size);

int message_recv_impl(int *max_size, uint8_t **data, int sock,
                      ReallocFunT realloc_fun)
{
    uint8_t nsize[4];
    int res, size;

    errno = 0;
    res = recv(sock, &nsize, sizeof(nsize), MSG_WAITALL);
    if (!res)
        return 0;
    if (res != sizeof(nsize))
        return -1;

    size = nsize[0]
         | (int)nsize[1] << 8
         | (int)nsize[2] << 16
         | (int)nsize[3] << 24;

    if (!size)
        return size;

    if (size > *max_size)
    {
        if (!realloc_fun)
        {
            errno = EFBIG;
            return -1;
        }
        if (realloc_fun(max_size, data, size * 2))
        {
            errno = ENOMEM;
            return -1;
        }
    }

    res = recv(sock, *data, size, MSG_WAITALL);
    if (!res)
        return 0;
    if (res != size)
        return -1;

    return size;
}

int message_recv_raw(int max_size, uint8_t *data, int sock)
{
    return message_recv_impl(&max_size, &data, sock, NULL);
}

static int _realloc_buf(int *max_size, uint8_t **data, int new_size)
{
    printf("Realloc for %d\n", new_size);
    *data = realloc(*data, new_size);
    if (!*data)
    {
        *max_size = 0;
        return -1;
    }
    *max_size = new_size;
    return 0;
}

int message_recv(struct Message *msg, int sock)
{
    int res = message_recv_impl(&msg->max_size, &msg->data, sock, _realloc_buf);
    if (res <= 0)
        return res;
    return msg->size = res;
}
