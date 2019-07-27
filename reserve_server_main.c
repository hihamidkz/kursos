#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "reserve_server.h"

#define BACKLOG 10
#define GET_PORT(port, num) port + num

/* Объединение, которое нужно для закрытия семафора */
union semun {
	int val;
	struct semid_ds *buf;
	unsigned short int* array;
	struct seminfo *__buf;
};

int main(int argc, char **argv)
{
	const char *multicast = "224.2.2.4";
	const int port = 3245;

	int prior;
	if (argc < 2) {
		prior = 1;
	} else {
		prior = atoi(argv[1]);
	}
	int sockfd;
	int count;

	/* Сокет для мультикаст-вещания */	
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	const int yes = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	inet_aton(multicast, &addr.sin_addr);

	struct ip_mreq mreq;
	inet_aton(multicast, &mreq.imr_multiaddr);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);

	setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

	if(bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("bind");
		exit(2);
	}
	/* END Сокет для мультикаст-вещания */

	recvfrom(sockfd, &count, sizeof(int), 0, NULL, NULL); // Получаем от главного сервера количество игр
	close(sockfd);

	/* reserve_server.c */
	int tcp_sock = get_tcp_socket(8080); // TCP-сокет нужен в дальнейшем, когда резервный сервер станет основным

	/* Создание семафора */
	key_t key = ftok("reserve_server", 'A');

	if (key < 0) {
		perror("ftok");
		exit(3);
	}

	int semid = semget(key, 1, IPC_CREAT | 0666);
	/* END Создание семафора */

	int pid = getpid();
	int my_number; // Номер процесса, 0, 1, 2...

	for (int i = 1; i < count; i++) {
		if (getpid() == pid) {
			fork();
			/* Код после форка выполняется всеми процессами, 
			 * так что дочерний процесс присвоит себе номер итерации цикла,
			 * а на следующей итерации выйдет из него
			*/
			my_number = i;
		} else {
			break;
		}
	}

	/* После всех итераций цикла, у процесса-родителя будет номер count - 1, так что обнуляем */
	if (getpid() == pid) {
		my_number = 0;
	}

	struct board gameboard;

	while (prior > 0) {
		/* reserve_server.c */
		int rez = monitor(GET_PORT(port, my_number), &gameboard);

		/* 
		 * 1 означает, что основной сервер упал
		 * уменьшаем prior на 1, если он стал 0
		 * становимся основным сервером
		*/
		if (rez == 1) {
			prior--;
			if (prior == 0)
				/* reserve_server.c */
				become_main_server(tcp_sock, my_number, semid, &gameboard);
		} else if (rez == 0) // 0 означает, что произошел выигрыш
			break;
	}

	close(sockfd);

	for (int i = 1; i < count; i++)
		wait(NULL);

	union semun ignored_argument; // Нужен для того, чтобы закрыть семафор, просто так
	semctl(semid, 1, IPC_RMID, ignored_argument);

	return 0;
}
