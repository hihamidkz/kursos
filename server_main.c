#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <pthread.h>

#include "interface.h"
#include "server.h"

#define TIMEOUT 60000
#define MSG_LEN 100
#define BACKLOG 10

union semun {
	int val;
	struct semid_ds *buf;
	unsigned short int* array;
	struct seminfo *__buf;
};

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
	
	int pid = getpid();
	
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
	
	key_t key = ftok("server", 'A');
	
	int semid = semget(key, 1, IPC_CREAT | 0666);
	
	if (semid < 0) {
		perror("semget");
		exit(1);
	}
	static struct sembuf sop_lock[2] = {
		0, 0, 0,
		0, 1, 0
	};
	static struct sembuf sop_unlock[1] = {0, -1, 0};
	
	if (key < 0) {
		perror("ftok");
		exit(1);
	}
	
	if (getpid() == pid) {
		for (int i = 1; i < count; i++)
			fork();
	}
	
	if (semop(semid, &sop_lock[0], 2) < 0) {
		perror("semop");
		exit(1);
	}
	if (clients_connect(sockfd, new_fd) < 0) {
		exit(1);
	}
	if (semop(semid, &sop_unlock[0], 1) < 0) {
		perror("semop");
		exit(1);
	}
	
	int nthreads = 2;
	pthread_t *tids = malloc(sizeof(*tids) * nthreads);
	struct board gameboard;
	bzero(gameboard.cells, 9 * sizeof(int));
	struct thread_data tdata1 = {&gameboard, 1, new_fd[0], new_fd[1]};
	struct thread_data tdata2 = {&gameboard, 2, new_fd[1], new_fd[0]};
	
	pthread_create(&tids[0], NULL, thread_func, &tdata1);
	pthread_create(&tids[1], NULL, thread_func, &tdata2);
	
	pthread_join(tids[0], NULL);
	pthread_join(tids[1], NULL);

	close(new_fd[0]);
	close(new_fd[1]);
	close(sockfd);
	
	for (int i = 0; i < count - 1; i++)
		wait(NULL);

	union semun ignored_argument;
	semctl(semid, 1, IPC_RMID, ignored_argument);
	return 0;
}





