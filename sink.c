#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>

#include "message.h"
#include "protocol.h"


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

static int ReceivePackets(int sock)
{
    struct Message *msg = message_alloc(1024);
    if (!msg)
    {
        fprintf(stderr, "Failed to alloc message\n");
        return -1;
    }

    while (1)
    {
        int size = protocol_packet_recv(sock, msg);
        if (-1 == size)
        {
            perror("Failed to receive");
            message_free(msg);
            return -1;
        }
        if (!size)
            break;

        printf("[%d] %d bytes\n", protocol_packet_get_type(msg),
                                  protocol_packet_get_size(msg));
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
    char buffer[1024];

    sock = Connect(host, port);
    if (-1 == sock)
    {
        fprintf(stderr, "Failed to connect\n");
        return 1;
    }

    if (-1 == protocol_init_send(sock, video, audio))
    {
        close(sock), sock = -1;
        return 1;
    }

    if (-1 == protocol_resp_recv(sock, buffer, sizeof(buffer)))
    {
        close(sock), sock = -1;
        return 1;
    }
    if (buffer[0])
    {
        fprintf(stderr, "Error: %s\n", buffer);
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
