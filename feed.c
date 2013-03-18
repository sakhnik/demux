#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <stdio.h>
#include <assert.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

volatile sig_atomic_t exit_flag = 0;

static void DumpInfo(AVFormatContext const *avf_context)
{
    int i;

    for (i = 0; i < avf_context->nb_streams; ++i)
    {
        AVCodecContext const *codec = avf_context->streams[i]->codec;

        printf("%d: %s %s [%s]\n",
               i, av_get_media_type_string(codec->codec_type),
               codec->codec_descriptor->name,
               codec->codec_descriptor->long_name);
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
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue;

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

static int AcceptClient(int sfd, AVFormatContext const *avf_context)
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

    // TODO: Read request
    // TODO: Send data

    close(infd);
    return 0;
}

static int RunServer(char const *port, AVFormatContext const *avf_context)
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
