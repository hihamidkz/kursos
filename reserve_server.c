#include <arpa/inet.h>
#include <malloc.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "interface.h"
#include "reserve_server.h"
#include "server.h"

#define BACKLOG 10
#define CLIENTS_NOT_FOUND 201
#define OK 200
#define TIMEOUT 60000
#define WIN 201
#define GET_PORT(port, num) port + num

/**
  * Функция для мониторинга основного сервера
**/
int monitor(int port, struct board *gameboard)
{
	const char *multicast = "224.2.2.4";
	int sockfd;

	/* Сокет для мультикаст-вещания */
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("socket");
		return -1;
	}
	
	const int yes = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(int));

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
		return -2;
	}
	/* END Сокет для мультикаст-вещания */

	struct pollfd pfd;
	pfd.fd = sockfd;
	pfd.events = POLLIN;

	int status;

	while (1) {	
		int rez = poll(&pfd, 1, TIMEOUT);

		if (rez > 0) {
			recvfrom(sockfd, &status, sizeof(int), 0, NULL, NULL);
			printf("status = %d\n", status);
			if (status == OK) {
				recvfrom(sockfd, gameboard, sizeof(*gameboard), 0, NULL, NULL);
			} else if (status == WIN || status == CLIENTS_NOT_FOUND) {
				break;
			} else {
				printf("The main server has failed.\n");
				close(sockfd);
				return 1;
			}
		} else if (rez == 0) {
			printf("The main server has failed.\n");
			close(sockfd);
			return 1;
		}
	}

	close(sockfd);
	return 0;
}

/**
  * Возвращает новый TCP-сокет
**/
int get_tcp_socket(int port_number)
{
	int sockfd;
	
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		return -1;
	}
	
	struct sockaddr_in my_addr;
	int yes = 1;
	
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		perror("setsockopt");
		return -2;
	}
	
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(int)) == -1) {
		perror("setsockopt");
		return -2;
	}

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(port_number);
	my_addr.sin_addr.s_addr = INADDR_ANY;
	memset(&(my_addr.sin_zero), 0, 8);

	if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
		perror("bind");
		return -3;
	}
	
	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		return -4;
	}

	return sockfd;
}

/**
  * После того, как резервный сервер стал основным,
  * ему нужно ждать подключения клиентов
**/
int wait_clients(int sockfd, int *new_fd)
{
	int clients_count = 0;
	struct sockaddr_in their_addr[2];
	
	struct pollfd pfd;
	pfd.fd = sockfd;
	pfd.events = POLLIN;
	while (clients_count < 2) {
		int poll_rez = poll(&pfd, 1, TIMEOUT);

		if (poll_rez < 0) {
			perror("poll");
			return -1;
		} else if (poll_rez > 0) {
			socklen_t sin_size = sizeof(struct sockaddr_in);
			if ((new_fd[clients_count] = accept(sockfd, (struct sockaddr *)&their_addr[clients_count], &sin_size)) == -1) {
				perror("accept");
				return -2;
			}
			
			clients_count++;
		}
	}
	
	return 0;
}

/**
  * Становимся главным сервером
  * sockfd - TCP-сокет, который мы создавали в main (reserve_server_main.c:71)
  * proc_number - номер процесса, нужен для выбора порта
  * semid - семафор (reserve_server_main.c:81)
**/
int become_main_server(int sockfd, int proc_number, int semid, struct board *gameboard)
{
	/* Код идентичен main главного сервера (server_main.c), та же логика */
	const char *multiaddr = "224.2.2.4";
	const int reserve_port = 3245;

	int reserve_sock;

	struct sockaddr_in reserve_addr;
	reserve_addr.sin_family = AF_INET;
	reserve_addr.sin_port = htons(reserve_port);
	inet_aton(multiaddr, &(reserve_addr.sin_addr));

	if ((reserve_sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("socket");
		return 1;
	}

	int new_fd[2];

	if (semop(semid, &sop_lock[0], 2) < 0) {
		perror("semop");
		return 2;
	}

	if (wait_clients(sockfd, new_fd) < 0) {
		return 3;
	}

	if (semop(semid, &sop_unlock[0], 1) < 0) {
		perror("semop");
		return 4;
	}

	int nthreads = 2;
	pthread_t *tids = malloc(sizeof(*tids) * nthreads);
	
	struct thread_data tdata1 = {gameboard, 1, new_fd[0], new_fd[1], GET_PORT(reserve_port, proc_number)};
	struct thread_data tdata2 = {gameboard, 2, new_fd[1], new_fd[0], GET_PORT(reserve_port, proc_number)};
	
	pthread_create(&tids[0], NULL, thread_func, &tdata1);
	pthread_create(&tids[1], NULL, thread_func, &tdata2);
	
	pthread_join(tids[0], NULL);
	pthread_join(tids[1], NULL);
	close(sockfd);
	close(reserve_sock);

	return 0;
}
