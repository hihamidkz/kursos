#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <arpa/inet.h>

#include "interface.h"
#include "server.h"

#define MSG_LEN 100
#define TIMEOUT 60000
#define DEAD_HEAT 4

/**
  * Ждем подключения клиентов
***/
int clients_connect(int sockfd, int *new_fd)
{
	int clients_count = 0;
	char message[MSG_LEN];
	struct sockaddr_in their_addr[2];
	
	while (clients_count < 2) {
		struct pollfd pfd;
		pfd.fd = sockfd;
		pfd.events = POLLIN;
		
		int poll_rez = poll(&pfd, 1, TIMEOUT);
		if (poll_rez < 0) {
			perror("poll");
			return -1;
		} else if (poll_rez == 0) {
			if (clients_count == 0)
				printf("Time limit exceeded. Clients not found.\n");
			else {
				sprintf(message, "Time limit exceeded. The second client is not found.");
				send(new_fd[0], message, strlen(message), 0);
				printf("Time limit exceeded. The second client is not found.\n");
			}
			return -1;
		}
		
		socklen_t sin_size = sizeof(struct sockaddr_in);
		if ((new_fd[clients_count] = accept(sockfd, (struct sockaddr *)&their_addr[clients_count], &sin_size)) == -1) {
			perror("accept");
			return -1;
		}
			
		clients_count++;

		if (clients_count == 1) {
			sprintf(message, "Hello %s.", inet_ntoa(their_addr[clients_count - 1].sin_addr));
			send(new_fd[clients_count - 1], message, strlen(message), 0);
			continue;
		}
		sprintf(message, "Hello. You are number two. The first client is %s", inet_ntoa(their_addr[clients_count - 1].sin_addr));
		send(new_fd[clients_count - 1], message, strlen(message), 0);
		sprintf(message, "Client number two is %s\nYour turn is first.", inet_ntoa(their_addr[clients_count - 1].sin_addr));
		send(new_fd[clients_count - 2], message, strlen(message), 0);
	}
	
	return 0;
}

/**
  * Проверяем, не произошла ли победа, если да
  * отправляем сообщения клиентам
***/
int win_verification(int player, int fd1, int fd2, struct board gameboard)
{
	int win = check_win(gameboard);
	char message[MSG_LEN];
			
	if (win > 0) {
		int msg_type = DATA;
		send(fd1, &msg_type, sizeof(int), 0);
		send(fd1, &win, sizeof(int), 0);
		send(fd2, &msg_type, sizeof(int), 0);
		send(fd2, &win, sizeof(int), 0);

		if (win == DEAD_HEAT) {
			sprintf(message, "Dead heat!");
			send(fd1, message, strlen(message), 0);
			send(fd2, message, strlen(message), 0);
		} else if (win == player) {
			sprintf(message, "You are winner!");
			send(fd1, message, strlen(message), 0);
			sprintf(message, "You are loser.");
			send(fd2, message, strlen(message), 0);
		} else {
			sprintf(message, "You are loser.");
			send(fd1, message, strlen(message), 0);
			sprintf(message, "You are winner!");
			send(fd2, message, strlen(message), 0);
		}
		
		return 1;
	}
	
	return 0;
}

void *thread_func(void *arg)
{
	struct thread_data *data = (struct thread_data*)arg;
	struct board *gameboard = data->gameboard;
	struct pollfd pfd;
	pfd.fd = data->fd1;
	pfd.events = POLLIN;
	
	int numbytes;
	int msg_type;
	int pos[2];
	char message[MSG_LEN];
	
	while (1) {
		int poll_rez = poll(&pfd, 1, TIMEOUT);
		if (poll_rez > 0) {
			if (recv(data->fd1, &msg_type, sizeof(int), 0) == 0) {
				if (check_win(*gameboard) > 0) {
					break;
				}
				msg_type = EXIT;
				send(data->fd2, &msg_type, sizeof(int), 0);
				break;
			}

			switch (msg_type) {
			case DATA:
				send(data->fd2, &msg_type, sizeof(int), 0) < 0;
				recv(data->fd1, pos, 2 * sizeof(int), 0);
				
				send(data->fd2, pos, 2 * sizeof(int), 0);
				recv(data->fd1, gameboard, sizeof(*gameboard), 0);
				break;
			case MESSAGE:
				bzero(message, MSG_LEN);
				send(data->fd2, &msg_type, sizeof(int), 0);
				recv(data->fd1, message, MSG_LEN, 0);
				send(data->fd2, message, MSG_LEN, 0);
				break;
			case EXIT:
				send(data->fd2, &msg_type, sizeof(int), 0);
				break;
			}
			
			if (win_verification(data->player, data->fd1, data->fd2, *gameboard)) {
				break;
			}
		}
	}
}

