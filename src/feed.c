#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <getopt.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#include "protocol.h"
#include "tcp.h"

volatile sig_atomic_t exit_flag = 0;

static void dump_info(AVFormatContext const *avf_context)
{
    int i;

    for (i = 0; i < avf_context->nb_streams; ++i)
    {
        AVCodecContext const *codec;
        AVCodecDescriptor const *codec_desc;

        codec = avf_context->streams[i]->codec;
        if (!codec)
            continue;
        printf("%d: %s", i, av_get_media_type_string(codec->codec_type));

        codec_desc = codec->codec_descriptor;
        if (codec_desc)
            printf(" %s [%s]", codec_desc->name, codec_desc->long_name);
        printf("\n");
    }
}

static int respond(int infd, int video, int audio,
                   AVFormatContext const *avf_context)
{
    if (video >= avf_context->nb_streams ||
        audio >= avf_context->nb_streams)
    {
        char const *error = "No such stream";
        protocol_resp_send(infd, error);
        return -1;
    }

    if (avf_context->streams[video]->codec->codec_type != AVMEDIA_TYPE_VIDEO)
    {
        char const *error = "Not a video stream";
        protocol_resp_send(infd, error);
        return -1;
    }

    if (avf_context->streams[audio]->codec->codec_type != AVMEDIA_TYPE_AUDIO)
    {
        char const *error = "Not an audio stream";
        protocol_resp_send(infd, error);
        return -1;
    }

    if (-1 == protocol_resp_send(infd, NULL))
        return -1;

    return 0;
}

static int process_client(int infd, AVFormatContext *avf_context)
{
    AVPacket pkt;
    int video = 0;
    int audio = 0;

    if (protocol_init_recv(infd, &video, &audio))
        return -1;

    if (respond(infd, video, audio, avf_context))
        return 0;

    if (av_seek_frame(avf_context, -1, 0, 0) < 0)
        return -1;

    while (!exit_flag && !av_read_frame(avf_context, &pkt))
    {
        int res;

        if (pkt.stream_index == video)
            res = protocol_packet_send(infd, PROTOCOL_PT_VIDEO,
                                       pkt.data, pkt.size);
        else if (pkt.stream_index == audio)
            res = protocol_packet_send(infd, PROTOCOL_PT_AUDIO,
                                       pkt.data, pkt.size);
        else
        {
            av_free_packet(&pkt);
            continue;
        }

        av_free_packet(&pkt);

        if (!res)
            return 0;
        if (res == -1)
        {
            perror("Failed to receive");
            return 0;
        }
    }

    return 0;
}

static int run_server(char const *port, AVFormatContext *avf_context)
{
    int sfd = tcp_listen(port);
    if (-1 == sfd)
        return -1;

    do
    {
        int infd = tcp_accept(sfd);
        if (!infd)
            break;
        if (infd < 0)
        {
            tcp_close(sfd);
            return -1;
        }

        if (process_client(infd, avf_context))
        {
            tcp_close(infd);
            tcp_close(sfd);
            return -1;
        }

        tcp_close(infd);
    }
    //while (!exit_flag);
    while (0);

    printf("Exiting\n");
    tcp_close(sfd);
    return 0;
}

static void SetExitFlag(int signum)
{
    exit_flag = 1;
}

static void init_signal(void)
{
    struct sigaction act;

    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = SetExitFlag;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);

    act.sa_handler = SIG_IGN;
    sigaction(SIGHUP, &act, NULL);
}

static void usage(void)
{
    printf("Usage:\n"
           "  feed [options] <file.mkv>\n"
           "Options:\n"
           "  -p,--port 12345\tTCP port to listen\n"
           "  -h,--help\t\tPrint this help\n");
}

int main(int argc, char *argv[])
{
    char const *port = "12345";
    char const *mkv_file = NULL;

    int c;
    AVFormatContext *avf_context = NULL;

    while (1)
    {
        static struct option long_options[] =
        {
            { "help", no_argument      , 0, 'h' },
            { "port", required_argument, 0, 'p' },
            { 0, 0, 0, 0 }
        };
        int option_index = 0;

        c = getopt_long(argc, argv, "hp:",
                        long_options, &option_index);
        if (c == -1)
            break;
        switch (c)
        {
        case 'p':
            port = optarg;
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

    if (optind + 1 != argc)
    {
        usage();
        return 1;
    }
    mkv_file = argv[optind];

    av_register_all();

    if (avformat_open_input(&avf_context, mkv_file, NULL, NULL) < 0)
    {
        fprintf(stderr, "Couldn't open file\n");
        return 1;
    }

    if (avformat_find_stream_info(avf_context, NULL) < 0)
    {
        fprintf(stderr, "Couldn't find stream information\n");
        avformat_close_input(&avf_context);
        return 1;
    }

    dump_info(avf_context);

    init_signal();

    if (run_server(port, avf_context) < 0)
    {
        avformat_close_input(&avf_context);
        return 1;
    }

    avformat_close_input(&avf_context);
    return 0;
}
