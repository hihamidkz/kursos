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
#include <time.h>
#include <poll.h>

#include "interface.h"
#include "myreadkey.h"
#include "myterm.h"

#define MESSAGE 1
#define DATA 2
#define EXIT 3
#define WIN 4
#define MSG_LEN 100
#define ANOTHER_PLAYER(my_number) my_number ^ 3
#define TOP_BORDER 29

int main(int argc, char **argv)
{
	char *cp = "127.0.0.1";
	int port = 7777;

	for (int i = 1; i < argc; i += 2) {
		if (strcmp(argv[i], "--game-server") == 0 || strcmp(argv[i], "-s") == 0) {
			cp = malloc(strlen(argv[i]) * sizeof(char));
			strcpy(cp, argv[i + 1]);
		} else if (strcmp(argv[i], "--game-server-port") == 0 || strcmp(argv[i], "-p") == 0) {
			port = atoi(argv[i + 1]);
		} else {
			fprintf(stderr, "Unknown argument '%s'\n", argv[i]);
			exit(0);
		}
	}

	srand(time(NULL));

	struct board gameboard;
	bzero(gameboard.cells, 9 * sizeof(int));

	gameboard.x = 1;
	gameboard.y = 1;
	gameboard.w = 51;
	gameboard.h = 26;
	gameboard.filled_cells = 0;

	printBoard(gameboard); // interface.c

	int sockfd, numbytes;

	struct sockaddr_in their_addr;
	struct in_addr addr;

	if (inet_aton(cp, &addr) == 0) {
		perror("inet_aton");
		exit(1);
	}

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(2);
	}

	their_addr.sin_family = AF_INET;
	their_addr.sin_port = htons(port);
	their_addr.sin_addr = addr;
	memset(&(their_addr.sin_zero), 0, 8);

	if (connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1) {
		perror("connect");
		exit(3);
	}
	
	char message[MSG_LEN] = {0};
	int my_move;
	int my_number;
	int win = -1;
	
	numbytes = recv(sockfd, message, MSG_LEN, 0);
	message[numbytes] = 0;
	printf("\n%s\n", message);

	/* Мне пришло сообщение "Hello IP", значит ждем следующего сообщения */
	if (strncmp(message, "Hello ", 6) == 0) {
		numbytes = recv(sockfd, message, MSG_LEN, 0);
		message[numbytes] = 0;
		printf("%s\n", message);
	}

	/* Пришло сообщение Time limit exceeded, значит второй игрок не нашелся */
	if (strncmp(message, "Time", 4) == 0)
		exit(1);
	
	int pos[2];
	enum tPosSign val; // Крестик или нолик?

	/* Пришло сообщение Client number two is IP, значит мой ход первый */
	if (strncmp(message, "Cli", 3) == 0) {
		my_move = 1;
		my_number = 1;
		val = TIC;
	} else {
		my_move = -1;
		my_number = 2;
		val = TAC;
	}
	
	struct pollfd pfd;
	pfd.fd = sockfd;
	pfd.events = POLLIN | POLLHUP | POLLRDNORM;
	
	int line = 32; // Переменная для псевдографики, чтобы контролировать, на какую строчку выводить сообщение
	int pol;
	
	int msg_type = DATA;

	/* Переменная для контроля, к какому резервному серверу подключаться
	 * используется при считывании IP-адреса из конфиг файла
	*/
	int reserve_number = 1;
	int tmp;
	
	int rows, cols;
	mt_getscreensize(&rows, &cols);
	rk_mytermsave(); // Сохраняем режим терминала, чтоб потом восстановиться
	
	while (1) {
		/* Если мой ход */
		if (my_move == 1) {
			/* Сделать ход (interface.c)
			 * 1 - решил закончить игру
			*/
			if ((tmp = editBoard(&gameboard, pos, val, &line, sockfd)) == 1) {
				break;
			} else if (tmp == 3) { // Сервер крякнулся
				close(sockfd); // Закрываем сокет, чтоб переподключиться, используя эту же переменную
				/* Переподключаемся на резервный сервер (interface.c)
				 * NON_ZERO - что-то пошло не так, завершаем игру
				*/
				if (reconnect(&sockfd, reserve_number) != 0) {
					printf("Connection severed.\n");
					break;
				}
				reserve_number++;
				continue;
			}
			
			line++;

			/* Все нормально, отправляем данные о сделанном ходе на сервер */
			msg_type = DATA;
			send(sockfd, &msg_type, sizeof(int), 0);
			send(sockfd, pos, 2 * sizeof(int), 0);
			send(sockfd, &gameboard, sizeof(gameboard), 0);
			my_move = -my_move;

			/* 2 - значит другой игрок вышел, завершаем игру */
			if (tmp == 2) {
				break;
			}
		} else { // Не мой ход
			if ((pol = poll(&pfd, 1, 60)) > 0) { // Ждем чего-нибудь от другого клиента
				if (recv(sockfd, &msg_type, sizeof(int), 0) == 0) { // Сервер крякнулся
					close(sockfd);
					if (reconnect(&sockfd, reserve_number) != 0) {
						printf("Connection severed.\n");
						break;
					}
					reserve_number++;
					continue;
				}
				if (msg_type == DATA) { // Другой игрок походил
					pos[0] = pos[1] = 0; // Не знаю, зачем я тут обнуляю, ну пусть будет
					recv(sockfd, pos, 2 * sizeof(int), 0); // Принимаем позицию
					setBoardPos(&gameboard, pos[0], pos[1], val ^ 3); // Ставим на это место крестик или нолик (interface.c)
					my_move = -my_move;
					line++;
				} else if (msg_type == MESSAGE) { // Сообщение в чат
					bzero(message, MSG_LEN);
					recv(sockfd, message, MSG_LEN, 0);
					printf("\E[MPlayer%d> %s\n", ANOTHER_PLAYER(my_number), message);
					line += 2;
					continue;
				} else { // Если не MESSAGE и не DATA, значит EXIT
					mt_gotoXY(line, 1);
					printf("\E[MClient %d exited.\n", ANOTHER_PLAYER(my_number));
					break;
				}
			}
		}
		
		mt_gotoXY(line, 1);
		printf("Please, wait. You can press TAB to switch to chat.\n");

		/* До сюда доходят оба, и тот, кто ходит, и другой игрок
		 * на всякий случай ждем еще 60 мс, вдруг нам что-то придет
		*/
		if (pol > 0 && poll(&pfd, 1, 60) > 0) {
			if (recv(sockfd, &msg_type, sizeof(int), 0) == 0) { // Сервер крякнулся
				close(sockfd);
				if (reconnect(&sockfd, reserve_number) != 0) {
					printf("Connection severed.\n");
					break;
				}
				reserve_number++;
				continue;
			}
			
			if (msg_type == MESSAGE) { // Сообщение в чат
				mt_gotoXY(line, 1);
				bzero(message, 100);
				recv(sockfd, message, 100, 0);
				printf("\E[MPlayer%d> %s\n", ANOTHER_PLAYER(my_number), message);
				line++;
			} else if (msg_type == DATA) { // Произошел выигрыш
				recv(sockfd, &win, sizeof(int), 0);
			
				if (win > 0) { // Победа или поражение?
					numbytes = recv(sockfd, message, 100, 0);
					message[numbytes] = 0;
					printf("\E[M%s\n", message);
					break;
				}
			} else { // EXIT
				mt_gotoXY(line, 1);
				printf("\E[MClient %d exited.\n", ANOTHER_PLAYER(my_number));
				break;
			}
		}
		
		rk_mytermregime(0, 1, 0, 0, 1); // Переключение терминала в канонический режим, чтоб работать с псевдографикой и ждать нажатия клавиши
		enum keys k;
		rk_readkey(&k); // Ждем нажатия клавиши 1мс
		rk_mytermrestore(); // Возвращение в нормальный режим
		if (k == TAB) { // Чат
			send_message(sockfd, &line, my_number); // interface.c
		} else if (k == ESC) {
			exit_game(sockfd); // interface.c
		}
		if (line >= rows - 2) { // Дошли до конца терминала, очищаем
			line = TOP_BORDER;
			printBoard(gameboard);
			mt_gotoXY(line, 1);
		}
	}

	rk_mytermrestore();
	close(sockfd);
	return 0;
}

