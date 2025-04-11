#include "network.h"
#include <netinet/in.h>
#include <string.h>

static struct message *waiting_message;
static size_t waiting_message_type;
static pthread_mutex_t mutex;
static pthread_cond_t cond;

static struct message_type_queue {
	void (*foo)(struct message *);
} message_type_queues[MAX_MESSAGES] = {};

static pthread_t server_thread_id;

static int server_is_running = 0;
static int listen_sock = -1;

static int server_port = -1;

void * server_thread(void * arg)
{
	LOG_NETWORK("Server thread started");

	// Enable asynchronous cancellation: forces the thread to be cancelled at any moment.
	// WARNING: This is dangerous because it can cancel the thread in the middle of a critical section.
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	listen_sock = -1;
	char listen_port[6]; // Listening port (as string)
	const char *interface = (char *)arg;

	struct sockaddr_in serveraddr = { 0 };
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(server_port);

	listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock < 0) {
		perror("socket");
		return NULL;
	}

	if (inet_pton(AF_INET, interface ? interface : "127.0.0.1",
		      &serveraddr.sin_addr) <= 0) {
		perror("inet_pton");
		close(listen_sock);
		return NULL;
	}

	char tmp_test[INET6_ADDRSTRLEN];
	inet_ntop(AF_INET, &serveraddr.sin_addr, tmp_test, INET6_ADDRSTRLEN);

	// Enable address reuse.
	int optval = 1;
	if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &optval,
		       sizeof(optval)) < 0) {
		perror("setsockopt");
		close(listen_sock);
		return NULL;
	}
	if (bind(listen_sock, (struct sockaddr *)&serveraddr,
		 sizeof(serveraddr)) < 0) {
		perror("bind");
		close(listen_sock);
		return NULL;
	}

	// Start listening for incoming connections.
	if (listen(listen_sock, 5) < 0) {
		perror("listen");
		close(listen_sock);
		return NULL;
	}

	// wake up the main thread
	pthread_mutex_lock(&mutex);
	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&mutex);

	while (server_is_running) {

		// Accept an incoming connection.
		struct sockaddr_storage client_addr;
		socklen_t addr_size = sizeof(client_addr);
		const int conn_sock = accept(listen_sock,
		                             (struct sockaddr *)&client_addr,
		                             &addr_size);
		if (conn_sock < 0) {
			perror("accept");
			close(listen_sock);
			return NULL;
		}

		size_t message_size = 0;
		if (recv(conn_sock, &message_size, sizeof(message_size),
		         0) != sizeof(
			    message_size)) {
			perror("read");
			close(conn_sock);
			close(listen_sock);
			return NULL;
		}

		struct message *message = malloc(message_size);

		if (recv(conn_sock, message, message_size, 0) < 0) {
			perror("read");
			close(conn_sock);
			close(listen_sock);
			return NULL;
		}

		size_t message_type = message->message_type;

		const struct sockaddr_in *s = (struct sockaddr_in *)&
			client_addr;
		// message->sender.port = ntohs(s->sin_port);

		inet_ntop(AF_INET, &s->sin_addr, message->sender.host,
		          sizeof(message->sender.host));

		int message_usage_counter = 0;


		int diff = message_type_queues[message_type].foo != NULL;

		if (message_type_queues[message->message_type].foo != NULL) {
			message_usage_counter++;
			message_type_queues[message->message_type].foo(message);
		}
		
		// Is user waiting on thread
		pthread_mutex_lock(&mutex);
		
		if (waiting_message_type == message_type) {
			waiting_message = copy_message(message, message_size);
			message_usage_counter++;
			pthread_cond_signal(&cond);
		}
		
		pthread_mutex_unlock(&mutex);
		
		free_message(message);
		ENSURE_WARNING_NETWORK(message_usage_counter > 0,
		               "Message type %lu receive but not used",
		               message_type);

		close(conn_sock);
	}
	close(listen_sock);
    return NULL;
}

void start_server(const int port, const char* interface)
{
	LOG_NETWORK("Starting server on port %i", port);
	server_port = port;
	for (int i = 0; i < MAX_MESSAGES; i++) {
		message_type_queues[i].foo = NULL;
	}

	server_is_running = 1;
	pthread_create(&server_thread_id, NULL, server_thread, (void *)interface);
	// wait until the server is started
	pthread_mutex_lock(&mutex);
	pthread_cond_wait(&cond, &mutex);
	pthread_mutex_unlock(&mutex);
}

void stop_server()
{
	LOG_NETWORK("Stopping server...");
	server_is_running = 0;

	if (pthread_cancel(server_thread_id) != 0) {
		perror("pthread_cancel");
	}

	if (listen_sock != -1) {
		close(listen_sock);
	}

	pthread_join(server_thread_id, NULL);

	for (int i = 0; i < MAX_MESSAGES; i++) {
		message_type_queues[i].foo = NULL;
	}
	LOG_NETWORK("Server stopped.");
}

int get_server_port()
{
	return server_port;
}

char * get_server_ip()
{
	struct sockaddr_in local_addr;
	socklen_t addr_len = sizeof(local_addr);

	if (getsockname(listen_sock, (struct sockaddr *)&local_addr,
			&addr_len) == -1) {
		perror("getsockname");
		return NULL;
	}

	char *ip = malloc(INET6_ADDRSTRLEN);
	inet_ntop(AF_INET, &local_addr.sin_addr, ip, INET6_ADDRSTRLEN);

	return ip;
}


void send_message(const struct node_id *dest,
                  struct message *message,
                  const size_t message_size)
{
	if (ENSURE_ERROR_NETWORK(dest != NULL, "Destination node required")) {
		return;
	}
	if (ENSURE_ERROR_NETWORK(dest->host != NULL, "Destination host required")) {
		return;
	}
	if (ENSURE_ERROR_NETWORK(dest->port != 0, "Destination port required")) {
		return;
	}



	// Complete the message with the sender's information
	message->sender.port = get_server_port();
    // message->sender.host is currently set at the reception of the message

	struct addrinfo hints, *servinfo, *p;
	int rv;

	// Buffer to hold the port number as string (max "65535")
	char port_str[6];

	// Convert port number to string
	snprintf(port_str, sizeof(port_str), "%i", dest->port);


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

	ssize_t sent_bytes = send(sockfd, &message_size, sizeof(message_size),
	                          0);
	if (sent_bytes != sizeof(message_size)) {
		perror("send datasize");
		close(sockfd);
		freeaddrinfo(servinfo);
		return;
	}

	sent_bytes = send(sockfd, message, message_size, 0);
	if (sent_bytes != message_size) {
		perror("send data");
		close(sockfd);
		freeaddrinfo(servinfo);
		return;
	}
	LOG_NETWORK("Message type %lu sent", message->message_type);

	// Clean up resources: close the socket and free the address info structure
	close(sockfd);
	freeaddrinfo(servinfo);
}

void addHandler(const size_t message_type,
                struct node_id *sender,
                void callBack(struct message *message))
{
	message_type_queues[message_type].foo = callBack;
}


struct message *wait_message(const size_t message_type,
                             struct node_id *sender)
{
	waiting_message_type = message_type;

	pthread_mutex_lock(&mutex);

	LOG_NETWORK("Waiting for message type %lu", message_type);

	pthread_cond_wait(&cond, &mutex);

	LOG_NETWORK("Message type %lu received", message_type);

	struct message *message = waiting_message;
	waiting_message = NULL;

	pthread_mutex_unlock(&mutex);

	return message;
}
