#ifndef RECV_THREAD_H
#define RECV_THREAD_H

#include <pthread.h>

void recv_thread_set_server_socket(int sock);
int recv_thread_start(pthread_t *thread);
int recv_thread_send_ready_state(int is_ready);
void recv_thread_stop(void);

#endif