#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>

#include "message.h"
#include "tcp.h"

struct Message *message_alloc(int size)
{
    struct Message *message;

    message = (struct Message *) malloc(sizeof(struct Message));
    if (!message)
        return NULL;

    message->data = malloc(size);
    if (!message->data)
    {
        free(message);
        return NULL;
    }
    message->size = size;

    return message;
}

void message_free(struct Message *message)
{
    if (message)
    {
        if (message->data)
            free(message->data);
        free(message);
    }
}

int message_send(struct Message const *message, int sock)
{
    return tcp_send(sock, message->data, message->size);
}

static int _realloc_buf(int *size, uint8_t **data, int new_size)
{
    *data = realloc(*data, new_size);
    if (!*data)
    {
        *size = 0;
        return -1;
    }
    *size = new_size;
    return 0;
}

int message_recv(struct Message *msg, int sock)
{
    int res = tcp_recv_realloc(sock, &msg->data, &msg->size, _realloc_buf);
    if (res <= 0)
        return res;
    return msg->size = res;
}
