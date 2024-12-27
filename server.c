#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
char *response_ok = "HTTP/1.1 200 OK\r\n\r\n";
char *response_not_found = "HTTP/1.1 404 Not Found\r\n\r\n";

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

	int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
	if (client_fd < 0)
	{
		printf("Accept failed: %s \n", strerror(errno));
		return 1;
	}
	printf("Client connected\n");

	// Read request from client
	char request_buffer[BUFFER_SIZE] = {0};

	if (read(client_fd, request_buffer, BUFFER_SIZE) < 0)
	{
		printf("Read failed: %s \n", strerror(errno));
		close(server_fd);
		return 1;
	}
	else
	{
		printf("Request from client:%s\n", request_buffer);
	}

	// Parse HTTP request
	char *path = strtok(request_buffer, " ");
	path = strtok(NULL, " ");

	if (strcmp(path, "/") == 0)
	{
		send(client_fd, response_ok, strlen(response_ok), 0);
	}
	else if (strncmp(path, "/echo/", 6) == 0)
	{
		char *echo_string = path + 6;

		int string_length = strlen(echo_string);

		char response[BUFFER_SIZE];
		int response_size = snprintf(response, sizeof(response),
									 "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n%s",
									 string_length, echo_string);

		if (response_size >= 0 && response_size < sizeof(response))
		{
			printf("Debug: Full Response:\n%s\n", response);
			if (send(client_fd, response, response_size, 0) < 0)
			{
				printf("Send failed: %s \n", strerror(errno));
			}
		}
		else
		{
			printf("Response buffer overflow or snprintf error\n");
		}
	}
	else
	{
		send(client_fd, response_not_found, strlen(response_not_found), 0);
	}

	close(client_fd);
	close(server_fd);

	return 0;
}
