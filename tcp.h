#pragma once

void tcp_close(int sock);
int tcp_listen(char const *port);

int tcp_accept(int sock);

int tcp_connect(char const *host, char const *port);
