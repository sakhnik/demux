#pragma once

#include <stdint.h>

/* Message holds dynamically allocated memory */
struct Message
{
    int size;
    uint8_t *data;
};

struct Message *message_alloc(int max_size);
void message_free(struct Message *message);

/* Send via a connected socket. Semantics of send(2). */
int message_send(struct Message const *message, int sock);
/* Receive fro a connected socket. Semantics of recv(2). */
int message_recv(struct Message *msg, int sock);
