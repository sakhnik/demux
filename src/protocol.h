#pragma once

/*
 * Communication protocol incapsulation.
 * Function pairs can be used simultaneously in both ends
 * to simplify the code.
 */

#include <stdint.h>

struct Message;

/* Initial request of the sink */
int protocol_init_send(int sock, int video, int audio);
int protocol_init_recv(int sock, int *video, int *audio);

/* Response of the feed about the availability of the streams */
int protocol_resp_send(int sock, char const *error);
int protocol_resp_recv(int sock, char *error, int max_size);

/* Packets with stream data */
typedef enum
{
    PROTOCOL_PT_VIDEO = 0,
    PROTOCOL_PT_AUDIO
} ProtocolPktType;

int protocol_packet_send(int sock, ProtocolPktType type,
                         uint8_t const *data, int size);

int protocol_packet_recv(int sock, struct Message *message);

/* Accessors to the data in the received message */
ProtocolPktType protocol_packet_get_type(struct Message const *message);
uint8_t const *protocol_packet_get_data(struct Message const *message);
int protocol_packet_get_size(struct Message const *message);
