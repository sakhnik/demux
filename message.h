#pragma once

#include <stdint.h>

struct Message
{
    int size;
    uint8_t *data;
};

struct Message *message_alloc(int max_size);
void message_free(struct Message *message);

int message_send(struct Message const *message, int sock);
int message_recv(struct Message *msg, int sock);
