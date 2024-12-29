#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>

#define BUFFER_SIZE 1024
char *response_ok = "HTTP/1.1 200 OK\r\n\r\n";
char *response_not_found = "HTTP/1.1 404 Not Found\r\n\r\n";

void handle_client(int client_fd);

int main()
{
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	printf("Logs from your program will appear here!\n");

	int server_fd, client_addr_len;
	struct sockaddr_in client_addr;

	// Create socket
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1)
	{
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}

	// Setting socket options
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
	{
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		return 1;
	}

	// Binding the socket
	struct sockaddr_in serv_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(4221),
		.sin_addr = {htonl(INADDR_ANY)},
	};

	if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0)
	{
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}

	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0)
	{
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}

	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);

	while (1)
	{
		int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
		if (client_fd < 0)
		{
			printf("Accept failed: %s \n", strerror(errno));
			continue;
		}
		printf("Client connected\n");

		pthread_t thread_id;
		if (pthread_create(&thread_id, NULL, (void *)handle_client, (void *)(intptr_t)client_fd) != 0)
		{
			printf("Failed to create thread: %s\n", strerror(errno));
			close(client_fd);
			continue;
		}
		pthread_detach(thread_id);
	}
	close(server_fd);

	return 0;
}

void handle_client(int client_fd)
{
	char request_buffer[BUFFER_SIZE] = {0};
	if (read(client_fd, request_buffer, BUFFER_SIZE) < 0)
	{
		printf("Read failed: %s \n", strerror(errno));
		close(client_fd);
		return;
	}
	printf("Request from client:%s\n", request_buffer);

	char *path = strtok(request_buffer, " ");
	path = strtok(NULL, " ");

	if (strcmp(path, "/") == 0)
	{
		send(client_fd, response_ok, strlen(response_ok), 0);
	}
	else if (strncmp(path, "/echo/", 6) == 0)
	{
		char *echo_string = path + 6;
		char response[BUFFER_SIZE];
		snprintf(response, sizeof(response),
				 "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s",
				 strlen(echo_string), echo_string);
		send(client_fd, response, strlen(response), 0);
	}
	else if (strncmp(path, "/user-agent/", 12) == 0)
	{
		strtok(NULL, "\r\n");
		strtok(NULL, "\r\n");
		char *userAgent = strtok(0, "\r\n") + 12;

		char response[BUFFER_SIZE];
		int response_size = snprintf(response, sizeof(response),
									 "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\n\r\n%s",
									 strlen(userAgent), userAgent);

		printf("Debug: Full Response:\n%s\n", response);
		if (send(client_fd, response, response_size, 0) < 0)
		{
			printf("Send failed: %s \n", strerror(errno));
		}
	}
	else
	{
		send(client_fd, response_not_found, strlen(response_not_found), 0);
	}

	close(client_fd);
}