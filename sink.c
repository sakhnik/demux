#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>

#include "message.h"

static int Connect(char const *host, char const *port)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s, sock;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    s = getaddrinfo(host, port, &hints, &result);
    if (s)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return -1;
    }

    sock = -1;
    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0)
            continue;

        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0)
            break;

        close(sock), sock = -1;
    }

    freeaddrinfo(result);
    return sock;
}

static int SendRequest(int video, int audio, int sock)
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

    if (message_send_raw(sizeof(data), data, sock) > 0)
        return 0;
    return -1;
}

static int ReceiveResponse(int sock)
{
    uint8_t data[1024];
    int res;

    res = message_recv_raw(sizeof(data) - 1, data, sock);
    if (-1 == res)
    {
        perror("Failed to receive response");
        return -1;
    }
    if (!res)
        return 0;
    data[res] = 0;
    fprintf(stderr, "Error: %s\n", data);
    return -1;
}

static int ReceivePackets(int sock)
{
    struct Message *msg = NULL;

    msg = message_alloc(1024);
    if (!msg)
    {
        fprintf(stderr, "Failed to alloc message\n");
        return -1;
    }

    while (1)
    {
        int size = message_recv(msg, sock);
        if (-1 == size)
        {
            perror("Failed to receive");
            message_free(msg);
            return -1;
        }
        if (!size)
            break;

        //printf("%d\n", msg->size);
    }

    message_free(msg);
    return 0;
}

int main(int argc, char *argv[])
{
    char const *host = "localhost";
    char const *port = "12345";
    int video = 0;
    int audio = 1;

    int sock;

    sock = Connect(host, port);
    if (-1 == sock)
    {
        fprintf(stderr, "Failed to connect\n");
        return 1;
    }

    if (-1 == SendRequest(video, audio, sock))
    {
        close(sock), sock = -1;
        return 1;
    }

    if (-1 == ReceiveResponse(sock))
    {
        close(sock), sock = -1;
        return 1;
    }

    if (-1 == ReceivePackets(sock))
    {
        close(sock), sock = -1;
        return 1;
    }

    close(sock), sock = -1;
    return 0;
}
