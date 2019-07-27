#include <arpa/inet.h>
#include <fcntl.h>
#include <semaphore.h>
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
#include <sys/time.h>
#include <sys/wait.h>
#include <pthread.h>

#include "interface.h"
#include "server.h"

#define TIMEOUT 60000
#define MSG_LEN 100
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
	int count = 1; // Количество одновременных игр
	int port_number = 7777; // Номер порта
	
	const char *multiaddr = "224.2.2.4"; // Мультикаст адрес
	const int reserve_port = 3245; // Порт резервного сервера
	
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

	/* UDP-сокет для общения с резервными серверами по multicast */
    int reserve_sock;
	
	struct sockaddr_in reserve_addr;
	reserve_addr.sin_family = AF_INET;
	reserve_addr.sin_port = htons(reserve_port);
	inet_aton(multiaddr, &(reserve_addr.sin_addr));
	
	if ((reserve_sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("socket");
		return 2;
	}
	
	/* Отправляем резервным серверам количество игр */
	if (sendto(reserve_sock, &count, sizeof(int), 0, (struct sockadrr *)&reserve_addr, sizeof(struct sockaddr)) < 0) perror("sendto");

	int sockfd, new_fd[2] = {0, 0}; // Сокет сервера и сокеты для общения с клиентами
	

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

	/* Тут используется семафор. Он нужен для того, чтобы не возникало такой ситуации, 
	что один клиент подсоединился к одному процессу, другой клиент - к другому, 
	и между ними не может быт установлена игра. */
	key_t key = ftok("server", 'A'); // Ключ, нужен для создания семафора
	
	if (key < 0) {
		perror("ftok");
		exit(1);
	}

	int semid = semget(key, 1, IPC_CREAT | 0666); // Создаем семафор
	
	if (semid < 0) {
		perror("semget");
		exit(1);
	}

	// Структура для блокирования семафора
	static struct sembuf sop_lock[2] = {
		0, 0, 0,
		0, 1, 0
	};

	// Структура для разблокирования семафора
	static struct sembuf sop_unlock[1] = {0, -1, 0};

	/* Далее на каждую игру создаем отдельный процесс */

	int pid = getpid(); // Сохраняем ID текущего процесса

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

	/* Блокируем семафор */
	if (semop(semid, &sop_lock[0], 2) < 0) {
		perror("semop");
		exit(1);
	}

	/* И ждем подключения клиентов
	 * Функция clients_connect в файле server.c
	*/
	if (clients_connect(sockfd, reserve_sock, new_fd, reserve_addr) < 0) {
		exit(1);
	}

	/* Разблокируем семафор */
	if (semop(semid, &sop_unlock[0], 1) < 0) {
		perror("semop");
		exit(1);
	}

	/* Создаем два потока для общения с каждым клиента, и начинается игра */
	int nthreads = 2;
	pthread_t *tids = malloc(sizeof(*tids) * nthreads);
	struct board gameboard;
	bzero(gameboard.cells, 9 * sizeof(int));
	struct thread_data tdata1 = {&gameboard, 1, new_fd[0], new_fd[1], GET_PORT(reserve_port, my_number)};
	struct thread_data tdata2 = {&gameboard, 2, new_fd[1], new_fd[0], GET_PORT(reserve_port, my_number)};

	/* thread_func в файле server.c */
	pthread_create(&tids[0], NULL, thread_func, &tdata1);
	pthread_create(&tids[1], NULL, thread_func, &tdata2);

	pthread_join(tids[0], NULL);
	pthread_join(tids[1], NULL);

	close(new_fd[0]);
	close(new_fd[1]);
	close(sockfd);
    close(reserve_sock);

	for (int i = 0; i < count - 1; i++)
		wait(NULL);

	union semun ignored_argument; // Нужен для того, чтобы закрыть семафор, просто так
	semctl(semid, 1, IPC_RMID, ignored_argument);

	return 0;
}


