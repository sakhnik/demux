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
        perror("bind");
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

int tcp_send(int sock, uint8_t const *data, int size)
{
    return tcp_send2(sock, data, size, NULL, 0);
}

int tcp_send2(int sock,
              uint8_t const *data0, int size0,
              uint8_t const *data1, int size1)
{
    int i, res, total_size = 0;
    int size = size0 + size1;
    uint8_t nsize[] =
    {
        size & 0xFF,
        (size >> 8) & 0xFF,
        (size >> 16) & 0xFF,
        (size >> 24) & 0xFF
    };

    struct iovec iov[] =
    {
        { .iov_base = nsize,            .iov_len = sizeof(nsize) },
        { .iov_base = (void *)data0,    .iov_len = size0         },
        { .iov_base = (void *)data1,    .iov_len = size1         },
    };

    struct msghdr mh = { 0 };
    mh.msg_iov = iov;
    mh.msg_iovlen = sizeof(iov) / sizeof(iov[0]);

    for (i = 0; i != mh.msg_iovlen; ++i)
        total_size += mh.msg_iov[i].iov_len;

    res = sendmsg(sock, &mh, 0);
    if (!res)
        return 0;
    if (total_size != res)
        return -1;

    return size;
}

int tcp_recv(int sock, uint8_t *data, int max_size)
{
    return tcp_recv_realloc(sock, &data, &max_size, NULL);
}

int tcp_recv_realloc(int sock, uint8_t **data, int *max_size,
                     ReallocFunT realloc_fun)
{
    uint8_t nsize[4];
    int res, size;

    errno = 0;
    res = recv(sock, &nsize, sizeof(nsize), MSG_WAITALL);
    if (!res)
        return 0;
    if (res != sizeof(nsize))
        return -1;

    size = nsize[0]
         | (int)nsize[1] << 8
         | (int)nsize[2] << 16
         | (int)nsize[3] << 24;

    if (!size)
        return size;

    if (size > *max_size)
    {
        if (!realloc_fun)
        {
            errno = EFBIG;
            return -1;
        }
        if (realloc_fun(max_size, data, size * 2))
        {
            errno = ENOMEM;
            return -1;
        }
    }

    res = recv(sock, *data, size, MSG_WAITALL);
    if (!res)
        return 0;
    if (res != size)
        return -1;

    return size;
}
