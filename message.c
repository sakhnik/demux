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

int message_send_raw2(int sock,
                      uint8_t const *data0, int size0,
                      uint8_t const *data1, int size1)
{
    int i, res, total_size = 0;
    int size = size0 + size1;
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
        { .iov_base = (void *)data0,    .iov_len = size0         },
        { .iov_base = (void *)data1,    .iov_len = size1         },
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

int message_send_raw(int sock, uint8_t const *data, int size)
{
    return message_send_raw2(sock, data, size, NULL, 0);
}

int message_send(int sock, struct Message const *message)
{
    return message_send_raw(sock, message->data, message->size);
}

typedef int (*ReallocFunT)(int *max_size, uint8_t **data, int new_size);

int message_recv_impl(int sock, uint8_t **data, int *max_size,
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

int message_recv_raw(int sock, uint8_t *data, int max_size)
{
    return message_recv_impl(sock, &data, &max_size, NULL);
}

static int _realloc_buf(int *max_size, uint8_t **data, int new_size)
{
    *data = realloc(*data, new_size);
    if (!*data)
    {
        *max_size = 0;
        return -1;
    }
    *max_size = new_size;
    return 0;
}

int message_recv(int sock, struct Message *msg)
{
    int res = message_recv_impl(sock, &msg->data, &msg->max_size, _realloc_buf);
    if (res <= 0)
        return res;
    return msg->size = res;
}
