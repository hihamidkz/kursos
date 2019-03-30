#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <math.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>

#include "interface.h"

#define TIMEOUT 60000
#define MSG_LEN 100
#define BACKLOG 10
#define MESSAGE 1
#define DATA 2
#define EXIT 3

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

int main(int argc, char **argv)
{
	int count = 1;
	int port_number = 7777;
	
	for (int i = 1; i < argc; i += 2) {
		if (strcmp(argv[i], "--count-games") == 0 || strcmp(argv[i], "-g") == 0) {
			count = atoi(argv[i + 1]);
		} else if (strcmp(argv[i], "--port-number") == 0 || strcmp(argv[i], "-n") == 0) {
			port_number = atoi(argv[i + 1]);
		} else {
			fprintf(stderr, "Unknown argument '%s'\n", argv[i]);
			exit(EXIT_SUCCESS);
		}
	}
	
	int sockfd, new_fd[2] = {0, 0};
	
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(EXIT_FAILURE);
	}
	
	struct sockaddr_in my_addr;
	int yes = 1;
	
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		perror("setsockopt");
		exit(1);
	}
	
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(port_number);
	my_addr.sin_addr.s_addr = INADDR_ANY;
	memset(&(my_addr.sin_zero), 0, 8);

	if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
		perror("bind");
		exit(1);
	}
	
	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}
	
	if (clients_connect(sockfd, new_fd) < 0) {
		exit(1);
	}
	
	char message[MSG_LEN];
	struct pollfd pfd[2];
	
	pfd[0].fd = new_fd[0];
	pfd[1].fd = new_fd[1];
	pfd[0].events = pfd[1].events = POLLIN | POLLHUP | POLLRDNORM;
	
	struct board gameboard;
	bzero(gameboard.cells, 9 * sizeof(int));
	
	int pos[2];
	int win = -1;
	int niters = 0;
	
	int turn = 0;
	int msg_type = 1;
	int exit_game = -1;
	
	while (win == -1) {
		if (poll(&pfd[turn], 1, 60) > 0) {
			if (recv(new_fd[turn], &msg_type, sizeof(int), 0) == 0) {
				send(new_fd[turn ^ 1], NULL, 0, 0);
				printf("Client number %d severed connection.\n", turn);
				exit(EXIT_FAILURE);
			}
			
			if (msg_type == DATA) {
				if (exit_game < 0) {
					send(new_fd[turn ^ 1], &msg_type, sizeof(int), 0);
					recv(new_fd[turn], pos, 2 * sizeof(int), 0);
					send(new_fd[turn ^ 1], pos, 2 * sizeof(int), 0);
					recv(new_fd[turn], &gameboard, sizeof(gameboard), 0);
					niters++;
					turn ^= 1;
				} else {
					recv(new_fd[turn], pos, 2 * sizeof(int), 0);
					msg_type = EXIT;
					recv(new_fd[turn], &gameboard, sizeof(gameboard), 0);
					send(new_fd[turn], &msg_type, sizeof(int), 0);
					break;
				}
			} else if (msg_type == MESSAGE) {
				bzero(message, 100);
				send(new_fd[turn ^ 1], &msg_type, sizeof(int), 0);
				recv(new_fd[turn], message, 100, 0);
				send(new_fd[turn ^ 1], message, 100, 0);
			} else {
				send(new_fd[turn ^ 1], &msg_type, sizeof(int), 0);
				//recv(new_fd[turn], &exit_game, sizeof(int), 0);
				break;
			}
			
			if ((win = check_win(gameboard)) > 0) {
				
				msg_type = DATA;
				
				send(new_fd[0], &msg_type, sizeof(int), 0);
				send(new_fd[1], &msg_type, sizeof(int), 0);
				
				send(new_fd[0], &win, sizeof(int), 0);
				send(new_fd[1], &win, sizeof(int), 0);
				if (win == 1) {
					sprintf(message, "You are winner!");
					send(new_fd[0], message, strlen(message), 0);
					sprintf(message, "You are loser.");
					send(new_fd[1], message, strlen(message), 0);
					break;
				} else if (win == 2) {
					sprintf(message, "You are winner!");
					send(new_fd[1], message, strlen(message), 0);
					sprintf(message, "You are loser.");
					send(new_fd[0], message, strlen(message), 0);
					break;
				}
			} else if (niters == 9) {
				win = 3;
				msg_type = 2;
				
				send(new_fd[0], &msg_type, sizeof(int), 0);
				send(new_fd[1], &msg_type, sizeof(int), 0);
				send(new_fd[0], &win, sizeof(int), 0);
				send(new_fd[1], &win, sizeof(int), 0);
				sprintf(message, "Dead heat!");
				send(new_fd[0], message, strlen(message), 0);
				send(new_fd[1], message, strlen(message), 0);
				break;
			}
		} else if (poll(&pfd[turn ^ 1], 1, 60) > 0 && exit_game < 0) {
			recv(new_fd[turn ^ 1], &msg_type, sizeof(int), 0);
			if (msg_type == MESSAGE) {
				bzero(message, 100);
				send(new_fd[turn], &msg_type, sizeof(int), 0);
				recv(new_fd[turn ^ 1], message, 100, 0);
				send(new_fd[turn], message, strlen(message), 0);
			} else {
				//recv(new_fd[turn ^ 1], &exit_game, sizeof(int), 0);
				continue;
			}
		}
	}
	
	close(new_fd[0]);
	close(new_fd[1]);
	close(sockfd);
	
	return 0;
}





