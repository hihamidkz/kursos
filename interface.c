#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "myterm.h"
#include "mybigchars.h"
#include "interface.h"
#include "myreadkey.h"

#define DATA 2
#define DEAD_HEAT 4
#define EXIT 3
#define MAX_IP_LENGTH 15 // Максимальная длина IPv4 адреса 15 символов
#define MAX_CONFIG_LENGTH 20 // Длина IP-адрес + пробел + порт
#define MESSAGE 1
#define SERVER_UNAVAILABLE 503

int check_win(struct board b)
{
	if (b.cells[0] == b.cells[1] && b.cells[0] == b.cells[2] && b.cells[0] != 0)
		return b.cells[0];
	else if (b.cells[0] == b.cells[3] && b.cells[0] == b.cells[6] && b.cells[0] != 0)
		return b.cells[0];
	else if (b.cells[0] == b.cells[4] && b.cells[0] == b.cells[8] && b.cells[0] != 0)
		return b.cells[0];
	else if (b.cells[1] == b.cells[4] && b.cells[1] == b.cells[7] && b.cells[1] != 0)
		return b.cells[1];
	else if (b.cells[2] == b.cells[5] && b.cells[2] == b.cells[8] && b.cells[2] != 0)
		return b.cells[2];
	else if (b.cells[2] == b.cells[4] && b.cells[2] == b.cells[6] && b.cells[2] != 0)
		return b.cells[2];
	else if (b.cells[3] == b.cells[4] && b.cells[3] == b.cells[5] && b.cells[3] != 0)
		return b.cells[3];
	else if (b.cells[6] == b.cells[7] && b.cells[6] == b.cells[8] && b.cells[6] != 0)
		return b.cells[6];

    if (b.filled_cells == 9)
		return DEAD_HEAT;

	return -1;
}

void printBoard(struct board gameboard)
{
	mt_clrscr();
	bc_box(gameboard.x, gameboard.y, gameboard.h, gameboard.w);
	
	int x, y;
	int ch[2];
	
	for (x = 0; x < 3; x++) {
		for (y = 0; y < 3; y++) {
			int idx = x * 3 + y;
			if (gameboard.cells[idx] == 0)
				continue;
			else if (gameboard.cells[idx] == TIC) {
				ch[0] = 405029505;
				ch[1] = 2168595480;
				bc_printbigchar(ch, gameboard.x + x * 9 + 1, gameboard.y + y * 17 + 1, 0, 0);
			} else if (gameboard.cells[idx] == TAC) {
				ch[0] = 2172748158;
				ch[1] = 2122416513;
				bc_printbigchar(ch, gameboard.x + x * 9 + 1, gameboard.y + y * 17 + 1, 0, 0);
			}
		}
	}
}

void setBoardPos(struct board *b, int x, int y, enum tPosSign val)
{
	int idx = x * 3 + y;
	int ch[2];
	
	b->cells[idx] = val;
	b->filled_cells += 1;
	
	if (val == TAC) {
		ch[0] = 2172748158;
		ch[1] = 2122416513;
	} else {
		ch[0] = 405029505;
		ch[1] = 2168595480;
	}
	bc_printbigchar(ch, b->x + x * 9 + 1, b->y + y * 17 + 1, 0, 0);
}

/**
  * Проверяем, а не пришло ли нам сообщение в чат
  * пока мы совершали ход, или не покинул ли игру другоЙ клиент
***/
int check_socket(int sockfd, int val, int *line)
{
	struct pollfd pfd;
	pfd.fd = sockfd;
	pfd.events = POLLIN;

	int msg_type;
	char message[100];

	if (poll(&pfd, 1, 60) > 0) {
		if (recv(sockfd, &msg_type, sizeof(int), 0) == 0) {
			return -1;
		}
		switch (msg_type) {
		case MESSAGE:
			bzero(message, 100);
			recv(sockfd, message, 100, 0);
			printf("\E[MPlayer%d> %s\n", val ^ 3, message);
			*line += 2;
			break;
		case EXIT:
			mt_gotoXY(*line, 1);
			printf("\E[MClient %d exited.\n", val ^ 3);
			return 1;
		}
	}
	
	return 0;
}

int send_message(int sockfd, int *line, int my_number)
{
	char message[100];
	int msg_type;

	mt_gotoXY(*line, 1);
	sprintf(message, "\E[MPlayer%d> ", my_number);
	write(1, message, strlen(message));
	bzero(message, 100);
	read(0, message, 100);

	if (strlen(message) > 0) {
		msg_type = MESSAGE;
		send(sockfd, &msg_type, sizeof(int), 0);
		send(sockfd, message, strlen(message), 0);
		*line += 1;
		mt_gotoXY(*line, 1);
	}

	return 0;
}

int exit_game(int sockfd)
{
	printf("Are you sure you want to exit? (Y/n)\n");
	enum keys k;
	rk_readkey(&k);

	if (k == KEYN) {
		return -1;
	}

	int msg_type = EXIT;
	send(sockfd, &msg_type, sizeof(int), 0);
	return 0;
}

int editBoard(struct board *b, int *pos, enum tPosSign val, int *line, int sockfd)
{
	int set = -1;
	
	enum keys k;
	
	rk_mytermsave();
	
	mt_gotoXY(*line, 1);
	char *buf = "Please, press F5 to make a move, TAB to switch to chat, ESC to exit.\n";
	write(1, buf, strlen(buf));
	while (1) {
		rk_mytermregime(0, 1, 0, 0, 1);
		rk_readkey(&k);
		rk_mytermrestore();
		if (k == F5) {
			break;
		} else if (k == TAB) {
			send_message(sockfd, line, val);
			continue;
		} else if (k == ESC) {
			int rez = exit_game(sockfd);
			if (rez == 0)
				return 1;
			else
				continue;
		} else {
			mt_gotoXY(*line, 1);
			write(1, "\E[M", 3);
			printf("Please, press F5 to make a move, TAB to switch to chat, ESC to exit.\n");
		}
		
		int rez = check_socket(sockfd, val, line);
		
		if (rez == 1) {
			return 2;
		} else if (rez == -1) {
			return 3;
		}
	}
	
	while (set == -1) {
		pos[0] = rand() % 3;
		pos[1] = rand() % 3;
		int idx = pos[0] * 3 + pos[1];
		if (b->cells[idx] == 0) {
			b->cells[idx] = val;
			setBoardPos(b, pos[0], pos[1], val);
			set = 1;
			break;
		}
	}

	return 0;
}

int send_notification(FILE *config)
{
	char cp[15];
	int port;
	int reserve_sock;
	int status = SERVER_UNAVAILABLE;
	
	fscanf(config, "%s %d", cp, &port);
	struct sockaddr_in reserve_addr;
	reserve_addr.sin_family = AF_INET;
	reserve_addr.sin_port = htons(port);
	inet_aton(cp, &(reserve_addr.sin_addr));
	
	if ((reserve_sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("socket");
		return 1;
	}
	
	sendto(reserve_sock, &status, sizeof(int), 0, (struct sockadrr *)&reserve_addr, sizeof(struct sockaddr));
	close(reserve_sock);
	return 0;
}

/*
 * Переменная для дескриптора сокета
 * и номер резервного сервера
*/
int reconnect(int *sockfd, int number)
{
	FILE *config = fopen("hosts.cfg", "r");

	char cp[MAX_IP_LENGTH]; // IP-шник резервника
	char buf[MAX_CONFIG_LENGTH]; // Строка из конфиг-файла, содержащая IP-адрес и порт
	int port;

	for (int i = 2; i < 2 * number; i++) {
		fgets(buf, MAX_CONFIG_LENGTH, config);
		if (feof(config)) {
			return 4;
		}
	}

	send_notification(config);
	fscanf(config, "%s %d", cp, &port);
	fclose(config);

	struct sockaddr_in their_addr;
	struct in_addr addr;

	if (inet_aton(cp, &addr) == 0) {
		perror("inet_aton");
		return 1;
	}

	if ((*sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		return 2;
	}

	their_addr.sin_family = AF_INET;
	their_addr.sin_port = htons(port);
	their_addr.sin_addr = addr;
	memset(&(their_addr.sin_zero), 0, 8);

	int conn = 0;

	for (int i = 0; i < 100 || !conn; i++) {
		if (connect(*sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1) {
			continue;
		}
		conn = 1;
	}

	return 0;
}
