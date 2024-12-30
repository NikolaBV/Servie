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

void handle_client(int client_socket);

int main()
{
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	int server_socket, client_address_length;
	struct sockaddr_in client_address;

	// Create socket
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket == -1)
	{
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}

	// Setting socket options
	int enable_reuse = 1;
	if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &enable_reuse, sizeof(enable_reuse)) < 0)
	{
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		return 1;
	}

	// Binding the socket
	struct sockaddr_in server_address = {
		.sin_family = AF_INET,
		.sin_port = htons(4221),
		.sin_addr = {htonl(INADDR_ANY)},
	};

	if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) != 0)
	{
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}

	int connection_backlog = 5;
	if (listen(server_socket, connection_backlog) != 0)
	{
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}

	printf("Waiting for a client to connect...\n");
	client_address_length = sizeof(client_address);

	while (1)
	{
		int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_length);
		if (client_socket < 0)
		{
			printf("Accept failed: %s \n", strerror(errno));
			continue;
		}
		printf("Client connected\n");

		pthread_t thread_id;
		if (pthread_create(&thread_id, NULL, (void *)handle_client, (void *)(intptr_t)client_socket) != 0)
		{
			printf("Failed to create thread: %s\n", strerror(errno));
			close(client_socket);
			continue;
		}
		pthread_detach(thread_id);
	}
	close(server_socket);

	return 0;
}

void handle_client(int client_socket)
{
	char request_buffer[BUFFER_SIZE] = {0};
	if (read(client_socket, request_buffer, BUFFER_SIZE) < 0)
	{
		printf("Read failed: %s \n", strerror(errno));
		close(client_socket);
		return;
	}
	printf("Request from client: %s\n", request_buffer);

	char *request_method = strtok(request_buffer, " ");
	char *request_path = strtok(NULL, " ");

	if (strcmp(request_path, "/") == 0)
	{
		send(client_socket, response_ok, strlen(response_ok), 0);
	}
	else if (strncmp(request_path, "/echo/", 6) == 0)
	{
		char *echo_message = request_path + 6;
		char response[BUFFER_SIZE];
		snprintf(response, sizeof(response),
				 "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s",
				 strlen(echo_message), echo_message);
		send(client_socket, response, strlen(response), 0);
	}
	else if (strncmp(request_path, "/user-agent/", 12) == 0)
	{
		strtok(NULL, "\r\n"); // Skip the first line
		strtok(NULL, "\r\n"); // Skip the second line
		char *user_agent = strtok(0, "\r\n") + 12;

		char response[BUFFER_SIZE];
		int response_length = snprintf(response, sizeof(response),
									   "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\n\r\n%s",
									   strlen(user_agent), user_agent);

		printf("Debug: Full Response:\n%s\n", response);
		if (send(client_socket, response, response_length, 0) < 0)
		{
			printf("Send failed: %s \n", strerror(errno));
		}
	}
	else if (strncmp(request_path, "/files/", 7) == 0)
	{
		FILE *requested_file;
		char file_path[BUFFER_SIZE];
		char *file_name = request_path + 7;

		snprintf(file_path, sizeof(file_path), "/home/nikolavalkov/Documents/Servie/tmp/%s", file_name);

		requested_file = fopen(file_path, "r");
		if (!requested_file)
		{
			printf("File not found or could not be opened\n");
			send(client_socket, response_not_found, strlen(response_not_found), 0);
			close(client_socket);
			return;
		}

		const char *file_content_type = "text/plain";
		if (strstr(file_name, ".css") != NULL)
		{
			file_content_type = "text/css";
		}
		else if (strstr(file_name, ".html") != NULL)
		{
			file_content_type = "text/html";
		}

		char read_buffer[BUFFER_SIZE];
		size_t bytes_read = 0;
		char *file_content = NULL;
		size_t total_file_size = 0;

		while ((bytes_read = fread(read_buffer, 1, sizeof(read_buffer), requested_file)) > 0) // Reads the requested_file up to the size of the read_buffer into the read_buffer
		{
			char *new_content_buffer = realloc(file_content, total_file_size + bytes_read); // Changes the size of file_content to the total_file_size + the size of the file we read in bytes
			if (!new_content_buffer)
			{
				printf("Memory allocation failed\n");
				free(file_content);
				fclose(requested_file);
				send(client_socket, response_not_found, strlen(response_not_found), 0);
				close(client_socket);
				return;
			}
			file_content = new_content_buffer;

			memcpy(file_content + total_file_size, read_buffer, bytes_read); // Copies bytes_read bytes from read_buffer into file_content, starting at the current end (file_content + total_file_size)
			total_file_size += bytes_read;									 // Set the total_file_size to the amount of bytes read from the file
		}
		fclose(requested_file);

		char response_headers[BUFFER_SIZE];
		int header_length = snprintf(response_headers, sizeof(response_headers),
									 "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\n\r\n",
									 file_content_type, total_file_size);

		if (send(client_socket, response_headers, header_length, 0) < 0)
		{
			printf("Send headers failed: %s \n", strerror(errno));
			free(file_content);
			close(client_socket);
			return;
		}

		if (send(client_socket, file_content, total_file_size, 0) < 0)
		{
			printf("Send content failed: %s \n", strerror(errno));
		}

		free(file_content);
	}

	else
	{
		send(client_socket, response_not_found, strlen(response_not_found), 0);
	}

	close(client_socket);
}
