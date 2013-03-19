#include <stdio.h>
#include <string.h>

#include "protocol.h"
#include "message.h"


int protocol_init_send(int sock, int video, int audio)
{
    uint8_t data[] =
    {
        video & 0xFF,
        (video >> 8) & 0xFF,
        (video >> 16) & 0xFF,
        (video >> 24) & 0xFF,

        audio & 0xFF,
        (audio >> 8) & 0xFF,
        (audio >> 16) & 0xFF,
        (audio >> 24) & 0xFF
    };

    if (message_send_raw(sock, data, sizeof(data)) > 0)
        return 0;
    return -1;
}

int protocol_init_recv(int sock, int *video, int *audio)
{
    uint8_t data[sizeof(uint32_t) * 2];
    int res;

    res = message_recv_raw(sock, data, sizeof(data));
    if (res != sizeof(data))
    {
        if (res)
            perror("Failed to receive");
        return -1;
    }
    *video = data[0]
           | (int)data[1] << 8
           | (int)data[2] << 16
           | (int)data[3] << 24;
    *audio = data[4]
           | (int)data[5] << 8
           | (int)data[6] << 16
           | (int)data[7] << 24;

    return 0;
}

int protocol_resp_send(int sock, char const *error)
{
    int size = error ? strlen(error) : 0;
    return message_send_raw(sock, (uint8_t const *)error, size);
}

int protocol_resp_recv(int sock, char *error, int max_size)
{
    int res = message_recv_raw(sock, (uint8_t *)error, max_size - 1);
    if (-1 == res)
    {
        perror("Failed to receive response");
        return -1;
    }
    error[res] = 0;
    return 0;
}

int protocol_packet_send(int sock, ProtocolPktType type,
                         uint8_t const *data, int size)
{
    uint8_t btype = type;
    return message_send_raw2(sock, &btype, sizeof(btype),
                             data, size);
}

int protocol_packet_recv(int sock, struct Message *message)
{
    return message_recv(sock, message);
}

ProtocolPktType protocol_packet_get_type(struct Message const *message)
{
    return message->data[0];
}

uint8_t const *protocol_packet_get_data(struct Message const *message)
{
    return message->data + 1;
}

int protocol_packet_get_size(struct Message const *message)
{
    return message->size - 1;
}
