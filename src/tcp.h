#pragma once

/*
 * tcp_ functions are used to simplify work with TCP for this particular
 * task
 */

void tcp_close(int sock);

/* Create listening socket, bind it to the port, and start listenint */
int tcp_listen(char const *port);

/* Accept a client */
int tcp_accept(int sock);

/* Create a socket and connect to the given host */
int tcp_connect(char const *host, char const *port);

/* Send a chunk of data as a bounded message */
int tcp_send(int sock, uint8_t const *data, int size);
int tcp_send2(int sock,
              uint8_t const *data0, int size0,
              uint8_t const *data1, int size1);

/* Receive a bounded message */
int tcp_recv(int sock, uint8_t *data, int max_size);

typedef int (*ReallocFunT)(int *size, uint8_t **data, int new_size);
int tcp_recv_realloc(int sock, uint8_t **data, int *max_size,
                     ReallocFunT realloc_fun);
