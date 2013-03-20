#pragma once

void tcp_close(int sock);
int tcp_listen(char const *port);
int tcp_accept(int sock);
int tcp_connect(char const *host, char const *port);

int tcp_send(int sock, uint8_t const *data, int size);
int tcp_send2(int sock,
              uint8_t const *data0, int size0,
              uint8_t const *data1, int size1);


int tcp_recv(int sock, uint8_t *data, int max_size);

typedef int (*ReallocFunT)(int *size, uint8_t **data, int new_size);
int tcp_recv_realloc(int sock, uint8_t **data, int *max_size,
                     ReallocFunT realloc_fun);
