#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 300

int flag = 0;
int sockfd = 0;
char name[20];
void clearGarbage(char *arr, int size)
{
	int i;
	for (i = 0; i < size; i++)
	{
		if (arr[i] == '\n')
		{
			arr[i] = '\0';
			break;
		}
	}
}

void catch_ctrl_c_and_exit(int sig)
{
	flag = 1;
}

void *send_msg_handler()
{
	char message[BUFFER_SIZE] = {};
	char buffer[BUFFER_SIZE] = {};

	while (1)
	{
		fgets(message, BUFFER_SIZE, stdin);
		clearGarbage(message, BUFFER_SIZE);
		if (strcmp(message, "exit") == 0)
		{
			break;
		}
		else
		{
			sprintf(buffer, "%s: %s\n", name, message);
			send(sockfd, buffer, strlen(buffer), 0);
		}

		memset(message, 0, sizeof(message));
		memset(buffer, 0, sizeof(buffer));
	}
	catch_ctrl_c_and_exit(2);
}

void *recv_msg_handler()
{
	char message[BUFFER_SIZE] = {};
	while (1)
	{
		int receive = recv(sockfd, message, BUFFER_SIZE, 0);
		if (receive > 0)
		{
			printf("%s", message);
		}
		else if (receive == 0)
		{
			break;
		}
		memset(message, 0, sizeof(message));
	}
}

int main(int argc, char **argv)
{

	int port = PORT;

	signal(SIGINT, catch_ctrl_c_and_exit);

	printf("Please enter your name: ");
	fgets(name, 20, stdin);
	clearGarbage(name, strlen(name));

	if (strlen(name) > 20 || strlen(name) < 2)
	{
		printf("Name must be less than 30 and more than 2 characters.\n");
		return EXIT_FAILURE;
	}

	/* Socket settings */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in address;
	address.sin_family = AF_INET;		  // Domain
	address.sin_addr.s_addr = INADDR_ANY; // Socket address
	address.sin_port = htons(port);		  // Socket Port

	// Connect to Server
	int err = connect(sockfd, (struct sockaddr *)&address, sizeof(address));
	if (err == -1)
	{
		printf("ERROR: connect\n");
		return EXIT_FAILURE;
	}

	// Send name
	send(sockfd, name, 20, 0);

	printf("--------------------------------WELCOME TO THREAD TALK--------------------------------\n");

	pthread_t sendThread;
	if (pthread_create(&sendThread, NULL, send_msg_handler, NULL) != 0)
	{
		printf("ERROR: pthread\n");
		return EXIT_FAILURE;
	}

	pthread_t recvThread;
	if (pthread_create(&recvThread, NULL, recv_msg_handler, NULL) != 0)
	{
		printf("ERROR: pthread\n");
		return EXIT_FAILURE;
	}

	while (1)
	{
		if (flag)
		{
			printf("\nLeaving ....\n");
			break;
		}
	}

	close(sockfd);

	return EXIT_SUCCESS;
}