#ifndef NET_H
#define NET_H

#include <stddef.h>

#include "../../common/protocol.h"

int connect_to_server(const char *server_ip, int listen_port, JoinResponse *out, int *out_server_sock);
int create_client_listener(int *out_port);

int recv_all(int sock, void *buffer, size_t size);
int send_all(int sock, const void *buffer, size_t size);

#endif