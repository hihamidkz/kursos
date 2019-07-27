#ifndef RESERVE_SERVER_H
#define RESERVE_SERVER_H

#include <sys/sem.h>

#include "interface.h"

static struct sembuf sop_lock[2] = {
	0, 0, 0,
	0, 1, 0
};
static struct sembuf sop_unlock[1] = {0, -1, 0};

int monitor(int port, struct board *gameboard);
int get_tcp_socket(int port_number);
int wait_clients(int sockfd, int *new_fd);
int become_main_server(int sockfd, int pid, int semid, struct board *gameboard);

#endif
