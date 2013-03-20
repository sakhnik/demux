#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "message.h"
#include "protocol.h"
#include "tcp.h"
#include "writer.h"


static int do_receive_packets(int sock,
                              Writer *video_writer, Writer *audio_writer)
{
    while (1)
    {
        int res, size;
        struct Message *msg = message_alloc(1024);
        if (!msg)
        {
            fprintf(stderr, "Failed to alloc message\n");
            return -1;
        }

        size = protocol_packet_recv(sock, msg);
        if (-1 == size)
        {
            perror("Failed to receive");
            message_free(msg);
            return -1;
        }
        if (!size)
        {
            message_free(msg);
            return 0;
        }

        switch (protocol_packet_get_type(msg))
        {
        case PROTOCOL_PT_VIDEO:
            res = writer_post(video_writer, msg);
            break;
        case PROTOCOL_PT_AUDIO:
            res = writer_post(audio_writer, msg);
            break;
        default:
            fprintf(stderr, "Unsupported packet type\n");
            message_free(msg);
            return -1;
        }
        if (res == -1)
        {
            fprintf(stderr, "Failed to deliver packet\n");
            message_free(msg);
            return -1;
        }
    }

    return 0;
}

static int receive_packets(int sock)
{
    Writer *video_writer, *audio_writer;

    video_writer = writer_init("video.out");
    if (!video_writer)
    {
        fprintf(stderr, "Failed to init video writer\n");
        return -1;
    }

    audio_writer = writer_init("audio.out");
    if (!audio_writer)
    {
        fprintf(stderr, "Failed to init audio writer\n");
        writer_close(video_writer);
        return -1;
    }

    if (do_receive_packets(sock, video_writer, audio_writer))
    {
        writer_close(audio_writer);
        writer_close(video_writer);
        return -1;
    }

    writer_wait_close(audio_writer);
    writer_wait_close(video_writer);

    return 0;
}

int main(int argc, char *argv[])
{
    char const *host = "localhost";
    char const *port = "12345";
    int video = 0;
    int audio = 1;

    int sock;
    char buffer[1024];

    sock = tcp_connect(host, port);
    if (-1 == sock)
    {
        fprintf(stderr, "Failed to connect\n");
        return 1;
    }

    if (-1 == protocol_init_send(sock, video, audio))
    {
        tcp_close(sock);
        return 1;
    }

    if (-1 == protocol_resp_recv(sock, buffer, sizeof(buffer)))
    {
        tcp_close(sock);
        return 1;
    }
    if (buffer[0])
    {
        fprintf(stderr, "Error: %s\n", buffer);
        tcp_close(sock);
        return 1;
    }

    if (-1 == receive_packets(sock))
    {
        tcp_close(sock);
        return 1;
    }

    tcp_close(sock);
    return 0;
}
