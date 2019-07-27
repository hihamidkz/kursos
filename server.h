#ifndef SERVER_H
#define SERVER_H

#define MESSAGE 1
#define DATA 2
#define EXIT 3

struct thread_data {
	struct board *gameboard;
	int player;
	int fd1;
	int fd2;
    int reserve_port;
};

int clients_connect(int sockfd, int reserve_sock, int *new_fd, struct sockaddr_in reserve_addr);
void *thread_func(void *arg);

#endif
