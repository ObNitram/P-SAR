#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>


#include "utils/logger.h"


static struct message *waiting_message;
static size_t waiting_message_type;
static pthread_mutex_t mutex;
static pthread_cond_t cond;

static struct message_type_queue {
	void (*foo)(struct message *);
} message_type_queues[MAX_MESSAGES] = {};


void server_thread()
{
	int listen_sock = -1;
	struct addrinfo hints, *res, *p;
	int rv;
	const char *listen_port = "5555"; // Listening port (as string)

	// Set up hints for getaddrinfo for a passive (server) socket.
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; // Allow IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
	hints.ai_flags = AI_PASSIVE; // Use the local IP

	if ((rv = getaddrinfo(NULL, listen_port, &hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return;
	}

	// Loop through all results and bind to the first we can.
	for (p = res; p != NULL; p = p->ai_next) {
		listen_sock = socket(p->ai_family, p->ai_socktype,
		                     p->ai_protocol);
		if (listen_sock < 0) {
			perror("socket");
			continue;
		}
		// Enable address reuse.
		int optval = 1;
		if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &optval,
		               sizeof(optval)) < 0) {
			perror("setsockopt");
			close(listen_sock);
			continue;
		}
		if (bind(listen_sock, p->ai_addr, p->ai_addrlen) < 0) {
			perror("bind");
			close(listen_sock);
			continue;
		}
		break; // Successfully bound.
	}

	if (p == NULL || listen_sock == -1) {
		fprintf(stderr, "Failed to bind listening socket on port %s\n",
		        listen_port);
		freeaddrinfo(res);
		return;
	}
	freeaddrinfo(res);

	// Start listening for incoming connections.
	if (listen(listen_sock, 5) < 0) {
		perror("listen");
		close(listen_sock);
		return;
	}

	// Accept an incoming connection.
	struct sockaddr_storage client_addr;
	socklen_t addr_size = sizeof(client_addr);
	const int conn_sock = accept(listen_sock,
	                             (struct sockaddr *)&client_addr,
	                             &addr_size);
	if (conn_sock < 0) {
		perror("accept");
		close(listen_sock);
		return;
	}

	size_t message_type = 0;
	if (recv(conn_sock, &message_type, sizeof(message_type), 0) != sizeof(
		    message_type)) {
		perror("read");
		close(conn_sock);
		close(listen_sock);
		return;
	}

	size_t message_size = 0;
	if (recv(conn_sock, &message_size, sizeof(message_size), 0) != sizeof(
		    message_size)) {
		perror("read");
		close(conn_sock);
		close(listen_sock);
		return;
	}

	struct message *message = malloc(sizeof(struct message) + message_size);
	message->message_type = message_type;
	message->message_size = message_size;

	if (message_size > 0) {
		if (recv(conn_sock, &message->message_data, message_size,
		         0) < 0) {
			perror("read");
			close(conn_sock);
			close(listen_sock);
			return;
		}
	}

	int message_usage_counter = 0;

	// Is user waiting on thread
	pthread_mutex_lock(&mutex);

	if (waiting_message_type == message_type) {
		waiting_message = copy_message(message);
		message_usage_counter++;
		pthread_cond_signal(&cond);
	}

	pthread_mutex_unlock(&mutex);

	if (message_type_queues[message_type].foo != NULL) {
		message_usage_counter++;
		message_type_queues[message_type].foo(message);
	} else {
		free_message(message);
	}

	ensure_warning(message_usage_counter > 0, "Message type %lu receive but not used", message_type);

	close(conn_sock);
	close(listen_sock);
}

void free_message(struct message *message)
{
	if (message != NULL) {
		// if (message->message_data != NULL) {
		// 	free(message->message_data);
		// }
		free(message);
	}
}

struct message *copy_message(const struct message *message)
{
	struct message *copy = malloc(
		sizeof(struct message) + message->message_size);
	memcpy(copy, message, sizeof(struct message) + message->message_size);
	return copy;
}

static pthread_t server_thread_id;

void start_server()
{
	for (int i = 0; i < MAX_MESSAGES; i++) {
		message_type_queues[i].foo = NULL;
	}

	pthread_create(&server_thread_id, NULL, server_thread, NULL);
}

void stop_server()
{
	pthread_join(server_thread_id, NULL);

	for (int i = 0; i < MAX_MESSAGES; i++) {
		message_type_queues[i].foo = NULL;
	}
}


void send_message(const size_t message_type,
                  const struct node_id *dest,
                  const void *data,
                  const size_t datasize)
{
	if (ensure_error(message_type != 0,
	                 "Message type must be different than 0")) {
		return;
	}

	if (ensure_error(dest != NULL, "Destination node required")) {
		return;
	}
	if (ensure_error(dest->host != NULL, "Destination host required")) {
		return;
	}
	if (ensure_error(dest->port != 0, "Destination port required")) {
		return;
	}

	struct addrinfo hints, *servinfo, *p;
	int rv;

	// Buffer to hold the port number as string (max "65535")
	char port_str[6];

	// Convert port number to string
	snprintf(port_str, sizeof(port_str), "%zu", dest->port);

	// Set up hints for getaddrinfo
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; // Allow IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM; // TCP stream sockets

	// Get address information for the destination host and port
	if ((rv = getaddrinfo(dest->host, port_str, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return;
	}

	int sockfd = -1;
	// Loop through all results and connect to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next) {
		// Create a socket
		sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (sockfd == -1) {
			perror("socket");
			continue;
		}

		// Attempt to connect to the server
		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			perror("connect");
			close(sockfd);
			continue;
		}
		break; // Successfully connected
	}

	// Check if we managed to connect
	if (p == NULL || sockfd == -1) {
		fprintf(stderr, "Failed to connect to %s:%s\n", dest->host,
		        port_str);
		freeaddrinfo(servinfo);
		return;
	}

	// TODO : Consider converting header fields to network byte order for portability.
	// For example, if size_t is 64-bit on your system, you might need to use explicit types and proper conversions.

	// Send the header first
	ssize_t sent_bytes = send(sockfd, &message_type, sizeof(message_type),
	                          0);
	if (sent_bytes != sizeof(message_type)) {
		perror("send header");
		close(sockfd);
		freeaddrinfo(servinfo);
		return;
	}

	// Send the message size
	sent_bytes = send(sockfd, &datasize, sizeof(datasize), 0);
	if (sent_bytes != sizeof(datasize)) {
		perror("send datasize");
		close(sockfd);
		freeaddrinfo(servinfo);
		return;
	}

	// Send the data payload (if any)
	if (datasize > 0 && data != NULL) {
		sent_bytes = send(sockfd, data, datasize, 0);
		if (sent_bytes != datasize) {
			perror("send data");
			close(sockfd);
			freeaddrinfo(servinfo);
			return;
		}
	}

	// Clean up resources: close the socket and free the address info structure
	close(sockfd);
	freeaddrinfo(servinfo);
}

void addHandler(size_t message_type,
                struct node_id *sender,
                void callBack(struct message *message))
{
	message_type_queues[message_type].foo = callBack;
}


struct message *wait_message(size_t expected_message_type,
                             struct node_id *sender)
{
	waiting_message_type = expected_message_type;

	pthread_mutex_lock(&mutex);

	pthread_cond_wait(&cond, &mutex);

	struct message *message = waiting_message;
	waiting_message = NULL;

	pthread_mutex_unlock(&mutex);

	return message;
}