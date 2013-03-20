#pragma once

#include <stdint.h>

struct Message
{
    int size;
    uint8_t *data;
};

struct Message *message_alloc(int max_size);
void message_free(struct Message *message);

int message_send_raw(int sock, uint8_t const *data, int size);
int message_send_raw2(int sock,
                      uint8_t const *data0, int size0,
                      uint8_t const *data1, int size1);
int message_send(int sock, struct Message const *message);

int message_recv_raw(int sock, uint8_t *data, int max_size);
int message_recv(int sock, struct Message *msg);
