#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <stdio.h>
#include <assert.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#include "message.h"

volatile sig_atomic_t exit_flag = 0;

static void DumpInfo(AVFormatContext const *avf_context)
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

static int CreateSocket(char const *port)
{
    //https://banu.com/blog/2/how-to-use-epoll-a-complete-example-in-c/
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s, sfd;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    s = getaddrinfo(NULL, port, &hints, &result);
    if (s)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        int yes = 1;

        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (-1 == setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)))
        {
            perror("setsockopt");
            close(sfd), sfd = -1;
            continue;
        }

        s = bind(sfd, rp->ai_addr, rp->ai_addrlen);
        if (!s)
            break; /* Bound successfully */

        close(sfd), sfd = -1;
    }

    if (!rp)
    {
        assert(sfd == -1);
        fprintf(stderr, "Couldn't bind\n");
        return -1;
    }

    freeaddrinfo(result);
    return sfd;
}

static int GetRequest(int infd, int *video, int *audio)
{
    uint8_t data[sizeof(uint32_t) * 2];
    int res;

    res = message_recv_raw(sizeof(data), data, infd);
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

static int Respond(int infd, int video, int audio,
                   AVFormatContext const *avf_context)
{
    if (video >= avf_context->nb_streams ||
        audio >= avf_context->nb_streams)
    {
        char const *error = "No such stream";
        message_send_raw(strlen(error), (uint8_t const *)error, infd);
        return -1;
    }

    if (avf_context->streams[video]->codec->codec_type != AVMEDIA_TYPE_VIDEO)
    {
        char const *error = "Not a video stream";
        message_send_raw(strlen(error), (uint8_t const *)error, infd);
        return -1;
    }

    if (avf_context->streams[audio]->codec->codec_type != AVMEDIA_TYPE_AUDIO)
    {
        char const *error = "Not an audio stream";
        message_send_raw(strlen(error), (uint8_t const *)error, infd);
        return -1;
    }

    if (-1 == message_send_raw(0, NULL, infd))
        return -1;

    return 0;
}

static int ProcessClient(int infd, AVFormatContext *avf_context)
{
    AVPacket pkt;
    int video = 0;
    int audio = 0;

    if (GetRequest(infd, &video, &audio))
        return -1;

    if (Respond(infd, video, audio, avf_context))
        return 0;

    while (!exit_flag && !av_read_frame(avf_context, &pkt))
    {
        int res;
        //uint8_t is_audio = 0;

        //if (pkt.stream_index == video)
        //    is_audio = 0;
        //else if (pkt.stream_index == audio)
        //    is_audio = 1;
        //else
        //{
        //    av_free_packet(&pkt);
        //    continue;
        //}

        printf("[%d] frame %d bytes\n", pkt.stream_index, pkt.size);
        res = message_send_raw(pkt.size, pkt.data, infd);
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

static int AcceptClient(int sfd, AVFormatContext *avf_context)
{
    struct sockaddr in_addr;
    socklen_t in_len = sizeof(in_addr);
    int infd;
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

    infd = accept(sfd, &in_addr, &in_len);
    if (-1 == infd)
    {
        if (errno == EINTR)
            return 0;
        perror("accept");
        return -1;
    }

    printf("Accepted connection %d", infd);
    if (!getnameinfo(&in_addr, in_len,
                     hbuf, sizeof(hbuf),
                     sbuf, sizeof(sbuf),
                     NI_NUMERICHOST | NI_NUMERICSERV))
    {
        printf(" (host=%s, port=%s)", hbuf, sbuf);
    }
    printf("\n");

    if (ProcessClient(infd, avf_context))
    {
        close(infd);
        return -1;
    }

    close(infd);
    return 0;
}

static int RunServer(char const *port, AVFormatContext *avf_context)
{
    int sfd;

    sfd = CreateSocket(port);
    if (-1 == sfd)
        return -1;

    if (-1 == listen(sfd, SOMAXCONN))
    {
        perror("listen");
        close(sfd);
        return -1;
    }

    while (!exit_flag)
    {
        if (AcceptClient(sfd, avf_context))
        {
            close(sfd);
            return -1;
        }
    }

    printf("Exiting\n");
    close(sfd);
    return 0;
}

static void SetExitFlag(int signum)
{
    exit_flag = 1;
}

static void InitSignal(void)
{
    struct sigaction act;

    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = SetExitFlag;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
}

int main(int argc, char *argv[])
{
    AVFormatContext *avf_context = NULL;

    if (argc != 2)
    {
        char const *slash = strrchr(argv[0], '/');
        char const *progname = slash ? ++slash : argv[0];
        fprintf(stderr, "Usage: %s <file.mkv>\n", progname);
        return 1;
    }

    av_register_all();

    if (avformat_open_input(&avf_context, argv[1], NULL, NULL) < 0)
    {
        fprintf(stderr, "Couldn't open stream\n");
        return 1;
    }

    if (avformat_find_stream_info(avf_context, NULL) < 0)
    {
        fprintf(stderr, "Couldn't find stream information\n");
        avformat_close_input(&avf_context);
        return 1;
    }

    DumpInfo(avf_context);

    InitSignal();

    if (RunServer("12345", avf_context) < 0)
    {
        avformat_close_input(&avf_context);
        return 1;
    }

    avformat_close_input(&avf_context);
    return 0;
}
