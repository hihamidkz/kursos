#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <poll.h>

#include "interface.h"
#include "myreadkey.h"
#include "myterm.h"

#define MESSAGE 1
#define DATA 2
#define EXIT 3

int main(int argc, char **argv)
{
	struct board gameboard;
	bzero(gameboard.cells, 9 * sizeof(int));
	
	gameboard.x = 1;
	gameboard.y = 1;
	gameboard.w = 51;
	gameboard.h = 26;
	
	printBoard(gameboard);
		
	int sockfd, numbytes;
	
	struct sockaddr_in their_addr; // Адрес сервера
	struct in_addr addr; // IP-адрес сервера
	char *cp; // IP-адрес в точечной нотации
	int port; // Номер порта
	
	if (argc < 2) {
		cp = "127.0.0.1";
		port = 7777;
	} else if (argc < 4) {
		if (strcmp(argv[1], "--game-server") == 0 || strcmp(argv[1], "-s") == 0) {
			cp = malloc(strlen(argv[1]) * sizeof(char));
			strcpy(cp, argv[2]);
			port = 7777;
		} else if (strcmp(argv[1], "--game-server-port") == 0 || strcmp(argv[1], "-p") == 0) {
			port = atoi(argv[2]);
			cp = "127.0.0.1";
		} else {
			fprintf(stderr, "Unknown argument.\n");
			exit(EXIT_FAILURE);
		}
	} else if (argc < 6) {
		if (strcmp(argv[1], "--game-server") == 0 || strcmp(argv[1], "-s") == 0) {
			cp = malloc(strlen(argv[1]) * sizeof(char));
			strcpy(cp, argv[2]);
		} else if (strcmp(argv[1], "--game-server-port") == 0 || strcmp(argv[1], "-p") == 0)
			port = atoi(argv[2]);
		else {
			fprintf(stderr, "Unknown argument.\n");
			exit(EXIT_FAILURE);
		}
		if (strcmp(argv[3], "--game-server") == 0 || strcmp(argv[3], "-s") == 0) {
			cp = malloc(strlen(argv[4]) * sizeof(char));
			strcpy(cp, argv[4]);
		} else if (strcmp(argv[3], "--game-server-port") == 0 || strcmp(argv[3], "-p") == 0)
			port = atoi(argv[4]);
		else {
			fprintf(stderr, "Unknown argument.\n");
			exit(EXIT_FAILURE);
		}
	} else {
		fprintf(stderr, "Too many arguments.\n");
		exit(EXIT_FAILURE);
	}
	
	if (inet_aton(cp, &addr) == 0) {
		perror("inet_aton");
		exit(EXIT_FAILURE);
	}
	
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}
	
	their_addr.sin_family = AF_INET;
	their_addr.sin_port = htons(port);
	their_addr.sin_addr = addr;
	memset(&(their_addr.sin_zero), 0, 8);

	if (connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1) {
		perror("connect");
		exit(EXIT_FAILURE);
	}
	
	char message[100] = {0};
	char buf[50];
	int my_move;
	int my_number;
	int win = -1;
	
	numbytes = recv(sockfd, message, 100, 0);
	message[numbytes] = 0;
	printf("\n%s\n", message);
	if (strncmp(message, "Hello ", 6) == 0) {
		numbytes = recv(sockfd, message, 100, 0);
		message[numbytes] = 0;
		printf("%s\n", message);
	}
	
	if (strncmp(message, "Time", 4) == 0)
		exit(EXIT_FAILURE);
	
	int pos[2];
	enum tPosSign val;
	if (strncmp(message, "Cli", 3) == 0) {
		my_move = 1;
		my_number = 1;
		val = TAC;
	} else {
		my_move = -1;
		my_number = 2;
		val = TIC;
	}
	
	struct pollfd pfd;
	pfd.fd = sockfd;
	pfd.events = POLLIN | POLLHUP | POLLRDNORM;
	
	int line = 32;
	int pol;
	int chat_mode = -1;
	
	int msg_type = DATA;
	int save_game = -1;
	int tmp;
	
	rk_mytermsave();
	while (1) {
		if (chat_mode == 1) {
			mt_gotoXY(line, 1);
			sprintf(buf, "\E[MPlayer%d> ", my_number);
			write(1, buf, strlen(buf));
			bzero(message, 100);
			read(0, message, 100);
			if (strlen(message) > 0) {
				msg_type = MESSAGE;
				send(sockfd, &msg_type, sizeof(int), 0);
				send(sockfd, message, strlen(message), 0);
				line++;
			}
			chat_mode = -1;
		}
		
		if (save_game == 1) {
			printf("Are you sure you want to exit? (Y/n)\n");
			enum keys k;
			rk_readkey(&k);
			if (k == KEYN) {
				save_game = -1;
				continue;
			}
			msg_type = EXIT;
			send(sockfd, &msg_type, sizeof(int), 0);
			//send(sockfd, &save_game, sizeof(int), 0);
			break;
		}
		
		if (my_move == 1) {
			if ((tmp = editBoard(&gameboard, &pos[0], &pos[1], &line)) == 1) {
				chat_mode = 1;
				continue;
			} else if (tmp == 2) {
				save_game = 1;
				continue;
			}
			
			msg_type = DATA;
			send(sockfd, &msg_type, sizeof(int), 0);
			send(sockfd, pos, 2 * sizeof(int), 0);
			send(sockfd, &gameboard, sizeof(gameboard), 0);
			my_move = -my_move;
		} else {
			if ((pol = poll(&pfd, 1, 60)) > 0) {
				if (recv(sockfd, &msg_type, sizeof(int), 0) == 0) {
					printf("Connection severed.\n");
					exit(EXIT_FAILURE);
				}
				if (msg_type == DATA) {
					recv(sockfd, pos, 2 * sizeof(int), 0);
					setBoardPos(&gameboard, pos[0], pos[1], val);
					my_move = -my_move;
					line++;
				} else if (msg_type == MESSAGE) {
					bzero(message, 100);
					recv(sockfd, message, 100, 0);
					printf("\E[MPlayer%d> %s\n", my_number ^ 3, message);
					line += 2;
					continue;
				} else {
					mt_gotoXY(line, 1);
					printf("\E[MClient %d exited.\n", my_number ^ 3);
					break;
				}
			}
		}
		
		mt_gotoXY(line, 1);
		printf("Please, wait. You can press TAB to switch to chat.\n");
		
		if (pol > 0 && poll(&pfd, 1, 60) > 0) {
			if (recv(sockfd, &msg_type, sizeof(int), 0) == 0) {
				printf("Connection severed.\n");
				exit(EXIT_FAILURE);
			}
			
			if (msg_type == MESSAGE) {
				mt_gotoXY(line, 1);
				bzero(message, 100);
				recv(sockfd, message, 100, 0);
				printf("\E[MPlayer%d> %s\n", my_number ^ 3, message);
				line++;
			} else if (msg_type == DATA) {
				recv(sockfd, &win, sizeof(int), 0);
			
				if (win > 0) {
					numbytes = recv(sockfd, message, 100, 0);
					message[numbytes] = 0;
					printf("\E[M%s\n", message);
					break;
				}
			} else {
				mt_gotoXY(line, 1);
				printf("\E[MClient %d exited.\n", my_number ^ 3);
				break;
			}
		}
		
		rk_mytermregime(0, 1, 0, 0, 1);
		enum keys k;
		rk_readkey(&k);
		rk_mytermrestore();
		if (k == TAB) {
			chat_mode = 1;
			continue;
		} else if (k == ESC) {
			save_game = 1;
		}
	}

	close(sockfd);
	return 0;
}






