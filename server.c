#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

#define MAX_CLIENTS 5
#define PORT 8080
#define BUFFER_SIZE 2048

int cliCount = 0;
static int clientId = 0;

typedef struct
{
	int sockfd;
	int clientId;
	char name[20];
} Client;

Client clients[MAX_CLIENTS];

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void clearGarbage(char *arr, int length)
{
	for (int i = 0; i < length; i++)
	{
		if (arr[i] == '\n')
		{
			arr[i] = '\0';
			break;
		}
	}
}

void addClient(Client cl)
{
	pthread_mutex_lock(&mutex);

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (!clients[i].sockfd)
		{
			clients[i] = cl;
			break;
		}
	}

	pthread_mutex_unlock(&mutex);
}

void removeClient(int clientId)
{
	pthread_mutex_lock(&mutex);

	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clients[i].sockfd)
		{
			if (clients[i].clientId == clientId)
			{
				clients[i].sockfd = 0;
				strcpy(clients[i].name, "");
				clients[i].clientId = 0;
				break;
			}
		}
	}

	pthread_mutex_unlock(&mutex);
}
void send_message(char *msg, int selfId)
{
	pthread_mutex_lock(&mutex);

	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (clients[i].sockfd)
		{
			if (clients[i].clientId != selfId)
			{
				if (send(clients[i].sockfd, msg, strlen(msg), 0) < 0)
				{
					printf("Unable to send msg\n");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&mutex);
}
void *handle_client(void *arg)
{
	pthread_detach(pthread_self());

	char buf[BUFFER_SIZE];
	char name[20];
	int leave_flag = 0;

	cliCount++;

	Client thisClient = *((Client *)arg);
	if (recv(thisClient.sockfd, name, 20, 0) <= 0 || strlen(name) < 2 || strlen(name) >= 20 - 1)
	{
		printf("Didn't enter the name.\n");
		leave_flag = 1;
	}
	else
	{
		strcpy(thisClient.name, name);
		sprintf(buf, "%s has joined\n", thisClient.name);
		printf("%s", buf);
		send_message(buf, thisClient.clientId);
	}

	memset(buf, 0, sizeof(buf));

	while (1)
	{
		if (leave_flag)
		{
			break;
		}

		int receive = recv(thisClient.sockfd, buf, BUFFER_SIZE, 0);

		if (receive > 0)
		{

			send_message(buf, thisClient.clientId);

			clearGarbage(buf, strlen(buf));
			printf("Sending Message%s \n", buf);
		}
		else if (receive == 0 || !strcmp(buf, "exit"))
		{
			sprintf(buf, "%s has left\n", thisClient.name);
			printf("%s", buf);
			send_message(buf, thisClient.clientId);
			leave_flag = 1;
		}
		else
		{
			printf("ERROR: -1\n");
			leave_flag = 1;
		}

		memset(buf, 0, sizeof(buf));
	}

	close(thisClient.sockfd);
	removeClient(thisClient.clientId);
	cliCount--;

	pthread_exit(NULL);
}

int main(int argc, char **argv)
{
	int port = PORT;
	int option = 1;
	int listenfd = 0, sockfd = 0;
	pthread_t thread;

	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in address;
	address.sin_family = AF_INET;		  // Domain
	address.sin_addr.s_addr = INADDR_ANY; // Socket address
	address.sin_port = htons(port);		  // Socket Port

	/* Ignore pipe signals */
	signal(SIGPIPE, SIG_IGN);

	if (setsockopt(listenfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char *)&option, sizeof(option)) < 0)
	{
		printf("ERROR: setsockopt failed");
		return EXIT_FAILURE;
	}

	/* Bind */
	if (bind(listenfd, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		printf("ERROR: Socket binding failed");
		return EXIT_FAILURE;
	}

	/* Listen */
	if (listen(listenfd, MAX_CLIENTS) < 0)
	{
		printf("ERROR: Socket listening failed");
		return EXIT_FAILURE;
	}

	printf("--------------------------------WELCOME TO THREAD TALK--------------------------------\n");

	while (1)
	{
		socklen_t clilen = sizeof(address);
		sockfd = accept(listenfd, (struct sockaddr *)&address, &clilen);

		/* Check if max clients is reached */
		if ((cliCount + 1) == MAX_CLIENTS)
		{
			printf("Max clients reached. Rejected: ");
			close(sockfd);
			continue;
		}

		/* Client settings */
		Client newClient;
		newClient.sockfd = sockfd;
		clientId++;
		newClient.clientId = clientId;

		addClient(newClient);
		pthread_create(&thread, NULL, &handle_client, (void *)&newClient);

		sleep(1);
	}

	return EXIT_SUCCESS;
}