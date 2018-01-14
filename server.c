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

#define BACKLOG 10
#define MESSAGE 1
#define DATA 2
#define EXIT 3

int main(int argc, char **argv)
{
	int count, port_number;
	
	if (argc < 2) {
		count = 1;
		port_number = 7777;
	} else if (argc < 4) {
		if (strcmp(argv[1], "--count-games") == 0 || strcmp(argv[1], "-g") == 0) {
			count = atoi(argv[2]);
			port_number = 7777;
		} else if (strcmp(argv[1], "--port-number") == 0 || strcmp(argv[1], "-n") == 0) {
			count = 1;
			port_number = atoi(argv[2]);
		} else {
			fprintf(stderr, "Unknown argument.\n");
			exit(EXIT_FAILURE);
		}
	} else if (argc < 6) {
		if (strcmp(argv[1], "--count-games") == 0 || strcmp(argv[1], "-g") == 0)
			count = atoi(argv[2]);
		else if (strcmp(argv[1], "--port-number") == 0 || strcmp(argv[1], "-n") == 0)
			port_number = atoi(argv[2]);
		else {
			fprintf(stderr, "Unknown argument.\n");
			exit(EXIT_FAILURE);
		}
		
		if (strcmp(argv[3], "--count-games") == 0 || strcmp(argv[3], "-g") == 0)
			count = atoi(argv[4]);
		else if (strcmp(argv[3], "--port-number") == 0 || strcmp(argv[3], "-n") == 0)
			port_number = atoi(argv[4]);
		else {
			fprintf(stderr, "Unknown argument.\n");
			exit(EXIT_FAILURE);
		}
	} else {
		fprintf(stderr, "Too many arguments.\n");
		exit(EXIT_FAILURE);
	}
	
	int i;
	int pid;
	
	int sockfd, new_fd[2] = {0, 0};
	
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(EXIT_FAILURE);
	}
	
	struct sockaddr_in my_addr; // Адрес текущего хоста (сервера)
	struct sockaddr_in their_addr[2]; // Адрес подключившегося хоста (клиента)
	socklen_t sin_size;
	struct sigaction sa;
	int yes = 1;
	
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}
	
	fcntl(sockfd, F_SETFL, O_NONBLOCK);
	
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
	
	for (i = 0; i < count - 1; i++)
		pid = fork();
	
	int clients_count = 0;
	char message[100];
	int contin = -1; // Continue saved game?
	
	srand(pid);
	if (pid > 0)
		sleep(5 + rand() % 5);
	
	while (clients_count < 2) {
		fd_set readset;
		FD_ZERO(&readset);
		FD_SET(sockfd, &readset);
		FD_SET(new_fd[0], &readset);
		FD_SET(new_fd[1], &readset);
		
		struct timeval timeout;
		timeout.tv_sec = 60;
		timeout.tv_usec = 0;
		
		int max_elem = fmax(new_fd[0], new_fd[1]);
		int mx = fmax(sockfd, max_elem);
		
		if (select(mx + 1, &readset, NULL, NULL, &timeout) <= 0) {
			//perror("select");
			if (clients_count == 0)
				printf("Time limit exceeded. Clients not found.\n");
			else {
				sprintf(message, "Time limit exceeded. The second client not found.");
				send(new_fd[0], message, strlen(message), 0);
				printf("Time limit exceeded. The second client not found.\n");
			}
			exit(EXIT_FAILURE);
		}
		
		if (FD_ISSET(sockfd, &readset)) {
			sin_size = sizeof(struct sockaddr_in);
			if ((new_fd[clients_count] = accept(sockfd, (struct sockaddr *)&their_addr[clients_count], &sin_size)) == -1) {
				perror("accept");
				exit(EXIT_FAILURE);
			}
			fcntl(new_fd[clients_count], F_SETFL, O_NONBLOCK);
			
			clients_count++;
		}
		if (clients_count == 1) {
			sprintf(message, "Hello %s.", inet_ntoa(their_addr[clients_count - 1].sin_addr));
			send(new_fd[clients_count - 1], message, strlen(message), 0);
			//recv(new_fd[clients_count - 1]
			continue;
		}
		sprintf(message, "Hello. You are number two. The first client is %s", inet_ntoa(their_addr[clients_count - 1].sin_addr));
		send(new_fd[clients_count - 1], message, strlen(message), 0);
		sprintf(message, "Client number two is %s\nYour turn is first.", inet_ntoa(their_addr[clients_count - 1].sin_addr));
		send(new_fd[clients_count - 2], message, strlen(message), 0);
	}
	
	struct pollfd pfd[2];
	
	pfd[0].fd = new_fd[0];
	pfd[1].fd = new_fd[1];
	pfd[0].events = pfd[1].events = POLLIN | POLLHUP | POLLRDNORM;
	
	struct board gameboard;
	bzero(gameboard.cells, 9 * sizeof(int));
	
	int pos[2];
	int win = -1;
	int niters = 0;
	
	int turn = 0; // Who's move
	int msg_type = 1;
	int exit_game = -1; // To save or not to save?
	
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
	
	//if (exit_game == 1)
		//save_game(gameboard, turn);
	
	close(new_fd[0]);
	close(new_fd[1]);
	close(sockfd);
	
	for (i = 0; i < count - 1; i++)
		wait(NULL);
	return 0;
}





