#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

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

    writer_close(audio_writer);
    writer_close(video_writer);
    return 0;
}

static void usage()
{
    printf("Usage:\n"
           "  sink [options] <video> <audio>\n"
           "Options:\n"
           "  -p,--port 12345\tTCP port to connect to\n"
           "  -H,--host localhost\tHost to connect to the feed\n"
           "  -h,--help\t\tPrint this help\n"
           "  <video> defaults to 0\n"
           "  <audio> defaults to 1\n");
}

int main(int argc, char *argv[])
{
    char const *host = "localhost";
    char const *port = "12345";
    int video = 0;
    int audio = 1;

    int c;
    int sock;
    char buffer[1024];

    while (1)
    {
        static struct option long_options[] =
        {
            { "help", no_argument,       0, 'h' },
            { "port", required_argument, 0, 'p' },
            { "host", required_argument, 0, 'H' },
            { 0, 0, 0, 0 }
        };
        int option_index = 0;

        c = getopt_long(argc, argv, "hp:H:",
                        long_options, &option_index);
        if (c == -1)
            break;
        switch (c)
        {
        case 'p':
            port = optarg;
            break;
        case 'H':
            host = optarg;
            break;
        case '?':
        case 'h':
            usage();
            return 0;
        default:
            usage();
            return 1;
        }
    }

    if (optind + 2 < argc)
    {
        usage();
        return 1;
    }
    if (optind < argc)
    {
        if (1 != sscanf(argv[optind], "%d", &video))
        {
            fprintf(stderr, "Invalid <video>\n");
            return 1;
        }
    }
    if (optind + 1 < argc)
    {
        if (1 != sscanf(argv[optind + 1], "%d", &audio))
        {
            fprintf(stderr, "Invalid <audio>\n");
            return 1;
        }
    }

    sock = tcp_connect(host, port);
    if (-1 == sock)
    {
        perror("Failed to connect");
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
