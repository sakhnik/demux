#include <unistd.h>
#include <netdb.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "tcp.h"

static int create_sock_and_bind(char const *port)
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

int tcp_listen(char const *port)
{
    int sfd = create_sock_and_bind(port);
    if (-1 == sfd)
        return -1;

    if (-1 == listen(sfd, SOMAXCONN))
    {
        perror("listen");
        close(sfd);
        return -1;
    }

    return sfd;
}

void tcp_close(int sock)
{
    if (-1 == close(sock))
        perror("close");
}

int tcp_accept(int sock)
{
    struct sockaddr in_addr;
    socklen_t in_len = sizeof(in_addr);
    int infd;
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

    infd = accept(sock, &in_addr, &in_len);
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

    return infd;
}

int tcp_connect(char const *host, char const *port)
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
