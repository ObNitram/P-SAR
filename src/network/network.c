//#define DISABLE_LOG

#include "network.h"
#include "../library.h"
#include "network/cond_var.h"
#include "utils/logger.h"
#include <utils/utils.h>

#include <pthread.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>

#define MAX_EVENT 10

struct thread_args {
	const char *interface;
	const int port;
	struct cond_var server_ready;
};

static void (*callbacks[NUMBER_OF_MSG_TYPE])(struct message *) = { NULL };
pthread_mutex_t callbacks_lock = PTHREAD_MUTEX_INITIALIZER;

static pthread_t server_thread_id;

static bool server_is_running = false;

static struct connection_entry {
	struct node_id node;
	int sockfd;
} * connection_buffer;
pthread_mutex_t con_buff_lock = PTHREAD_MUTEX_INITIALIZER;

static size_t buffer_size;

int epollfd;

/// @brief Search for a given node if a socket is in the connection_buffer cache.
/// @param node Pointer to the node identifier to search.
/// @return The socket associated to this node or -1 if not found.
static int find_connection(const struct node_id *node)
{
	pthread_mutex_lock(&con_buff_lock);
	for (int i = 0; i < buffer_size; i++) {
		if (node_equal(&connection_buffer[i].node, node)) {
			int sock = connection_buffer[i].sockfd;
			pthread_mutex_unlock(&con_buff_lock);
			log_info("find sock %d for %s:%d",
				 connection_buffer[i].sockfd,
				 connection_buffer[i].node.host,
				 connection_buffer[i].node.port);
			return sock;
		}
	}
	pthread_mutex_unlock(&con_buff_lock);
	return -1;
}

/// @brief Add the tuple node/key to the connection_buffer cache.
/// @param node The node to add as a key.
/// @param socket The socket associated to the node.
/// @return socket on success or the socket already in the buffer
static int add_connection(const struct node_id node, const int socket)
{
	pthread_mutex_lock(&con_buff_lock);

	for (int i = 0; i < buffer_size; i++) {
		if (node_equal(&connection_buffer[i].node, &node)) {
			int sock = connection_buffer[i].sockfd;
			pthread_mutex_unlock(&con_buff_lock);
			log_info("node %s:%d have sock %d", node.host,
				 node.port, sock);
			return sock;
		}
	}

	buffer_size++;
	if (connection_buffer) {
		connection_buffer =
			realloc(connection_buffer,
				sizeof(struct connection_entry) * buffer_size);
	} else {
		connection_buffer =
			malloc(sizeof(struct connection_entry) * buffer_size);
	}

	log_info("add %d for %s:%d to buffer", socket, node.host, node.port);

	connection_buffer[buffer_size - 1].node = node;
	connection_buffer[buffer_size - 1].sockfd = socket;
	pthread_mutex_unlock(&con_buff_lock);

	return socket;
}

/// @brief Replace the socket associated to the given node in the connection_buffer cache.
/// @param node The node to search.
/// @param socket The new socket to replace.
/// @return socket en success or the valid socket in buffer
static int replace_socket(const struct node_id *node, const int socket)
{
	pthread_mutex_lock(&con_buff_lock);
	int i = 0;
	for (i = 0; i < buffer_size; i++) {
		if (node_equal(&connection_buffer[i].node, node)) {
			if (fcntl(connection_buffer[i].sockfd, F_GETFL) < 0 &&
			    errno == EBADF) {
				connection_buffer[i].sockfd = socket;
				break;
			}
			int sock = connection_buffer[i].sockfd;
			pthread_mutex_unlock(&con_buff_lock);
			return sock;
		}
	}
	pthread_mutex_unlock(&con_buff_lock);
	return socket;
}

static void *exec_handler(void *arg)
{
	struct message *message = (struct message *)arg;

	void (*callback)(struct message *) = NULL;

	pthread_mutex_lock(&callbacks_lock);
	if (message->message_type < NUMBER_OF_MSG_TYPE &&
	    callbacks[message->message_type] != NULL) {
		callback = callbacks[message->message_type];
	}
	pthread_mutex_unlock(&callbacks_lock);

	if (callback != NULL)
		callback(message);
	free(message);
	return NULL;
}

/// @brief Receive N byte of data on the given socket, protect to signal.
/// @param sock The socket to read.
/// @param data the data to write data, it must be initialize before with the given size.
/// @param size The size to read in the socket.
/// @return the size read or -1 on failure, a size of 0 means the end of communication on this socket.
static int Recv_all(const int sock, void *data, const size_t size)
{
	int seek = 0;
	int ret = 0;
	do {
		ret = recv(sock, data + seek, size - seek, 0);
		if (ret == 0 && seek == 0)
			break;
		if (ret == -1)
			return -1;
		seek += ret;
	} while ((size - seek) > 0);
	return seek;
}

void *server_thread(void *arg)
{
	log_info("Server thread started");

	const int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock < 0) {
		perror("socket");
		return NULL;
	}

	struct epoll_event ev, events[MAX_EVENT];

	//this block initiate the server so all local var wont be used in the next
	{
		//this will only work because main thread is waiting
		// after notify the main thread, param will be lost => access to it will segfault
		struct thread_args *param = (struct thread_args *)arg;

		struct sockaddr_in serveraddr = { 0 };
		serveraddr.sin_family = AF_INET;
		serveraddr.sin_port = htons(param->port);

		if (inet_pton(AF_INET,
			      param->interface ? param->interface : LOCALHOST,
			      &serveraddr.sin_addr) <= 0) {
			perror("inet_pton");
			close(listen_sock);
			return NULL;
		}

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

		// initiate me node => ip:port is not a valid id
		inet_ntop(AF_INET, &serveraddr.sin_addr, me.host,
			  INET6_ADDRSTRLEN);
		me.port = ntohs(serveraddr.sin_port);

		epollfd = epoll_create1(0);
		if (epollfd < 0) {
			perror("epoll");
			close(listen_sock);
			return NULL;
		}

		ev.events = EPOLLIN;
		ev.data.fd = listen_sock;

		if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) {
			perror("epollctl");
			close(listen_sock);
			close(epollfd);
			return NULL;
		}

		server_is_running = true;

		//wakeup main thread
		pthread_mutex_lock(&param->server_ready.lock);
		param->server_ready.predicate = true;
		pthread_cond_signal(&param->server_ready.cond);
		pthread_mutex_unlock(&param->server_ready.lock);
	}

	while (server_is_running) {
		//this epoll_wait must be the only cancelation point of the server
		// pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

		//wait event on epoll => pwait and mask signal ??
		int nevents = epoll_wait(epollfd, events, MAX_EVENT, -1);
		if (nevents == -1) {
			perror("epoll_wait");
			break;
		}

		// pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

		for (int i = 0; i < nevents; i++) {
			//if event on listen_sock then accept connection
			if (events[i].data.fd == listen_sock) {
				struct sockaddr_in client_addr;
				socklen_t addr_size = sizeof(client_addr);
				const int conn_sock =
					accept(listen_sock,
					       (struct sockaddr *)&client_addr,
					       &addr_size);

				if (conn_sock < 0) {
					perror("accept");
					close(listen_sock);
					continue;
				}

				//add socket to epoll
				ev.events = EPOLLIN;
				ev.data.fd = conn_sock;
				if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock,
					      &ev) == -1) {
					perror("epoll_ctl");
					close(conn_sock);
					continue;
				}

			} else {
				//if not an accept receive message
				size_t message_size = 0;

				int ret = Recv_all(events[i].data.fd,
						   &message_size,
						   sizeof(message_size));
				if (ret == -1) {
					perror("read size");
					close(events[i].data.fd);
					continue;
				}

				if (ret == 0) {
					close(events[i].data.fd);
					continue;
				}

				struct message *message = malloc(message_size);
				if (Recv_all(events[i].data.fd, (char *)message,
					     message_size) == -1) {
					log_error("sock %d", events[i].data.fd);
					perror("read data");
					close(events[i].data.fd);
					continue;
				}

				if (message->message_type >= NUMBER_OF_MSG_TYPE)
					continue;

				add_connection(message->sender,
					       events[i].data.fd);

				pthread_t handler;
				pthread_create(&handler, NULL, exec_handler,
					       (void *)message);
				pthread_detach(handler);
			}
		}
	}

	server_is_running = false;
	close(listen_sock);

	log_info("server exit successfuly");
	return NULL;
}

void start_server(const int port, const char *interface)
{
	log_info("Starting server on port %i with interface %s", port,
		 interface);

	// init value
	buffer_size = 0;
	connection_buffer = NULL;

	struct thread_args args = { .interface = interface,
				    .port = port,
				    .server_ready = COND_VAR_INIT };

	pthread_mutex_lock(&args.server_ready.lock);

	pthread_create(&server_thread_id, NULL, server_thread, (void *)&args);

	// wait until the server is started
	while (!args.server_ready.predicate)
		pthread_cond_wait(&args.server_ready.cond,
				  &args.server_ready.lock);

	pthread_mutex_unlock(&args.server_ready.lock);
}

void stop_server()
{
	log_info("Stopping server...");
	server_is_running = false;

	const int poisonous_sock = socket(AF_INET, SOCK_STREAM, 0);

	// TODO: kill the thread if poisonous injection fail
	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(me.port);
	inet_pton(AF_INET, me.host, &serv_addr.sin_addr);

	connect(poisonous_sock, (struct sockaddr *)&serv_addr,
		sizeof(serv_addr));
	close(poisonous_sock);

	pthread_join(server_thread_id, NULL);

	for (int i = 0; i < NUMBER_OF_MSG_TYPE; i++) {
		callbacks[i] = NULL;
	}

	for (int i = 0; i < buffer_size; i++) {
		close(connection_buffer[i].sockfd);
	}
	close(epollfd);

	buffer_size = 0;
	free(connection_buffer);

	log_info("server is stop");
}

int get_server_port()
{
	return me.port;
}

char *get_server_ip()
{
	char *ip = malloc(INET6_ADDRSTRLEN);
	memcpy(ip, me.host, INET6_ADDRSTRLEN);

	return ip;
}

/// @brief Send N byte of data on the given socket, protect to signal.
/// @param sock The socket to send.
/// @param data the data to write data, it must be initialize before with the given size.
/// @param size The size of data to send in the socket.
/// @return 0 on success or -1 on failure.
static int send_all(const int sockfd, const char *data, const size_t size)
{
	int seek = 0;
	int ret = 0;
	do {
		ret = send(sockfd, data + seek, size - seek, 0);
		if (ret == -1)
			return -1;
		seek += ret;
	} while ((size - seek) > 0);
	return 0;
}

static int send_message_internal(int sockfd, struct message *message,
				 const size_t message_size)
{
	if (send_all(sockfd, (char *)&message_size, sizeof(message_size)) ==
	    -1) {
		log_error("bad sock %d", sockfd);
		perror("send datasize");
		close(sockfd);
		return -1;
	}

	if (send_all(sockfd, (char *)message, message_size) == -1) {
		log_error("data failed");
		perror("send data");
		close(sockfd);
		return -1;
	}

	log_info("send success");

	return 0;
}

void send_message(const struct node_id *dest, struct message *message,
		  const size_t message_size)
{
	//It will be prettier to define return value for error handling
	if (ensure_error(dest != NULL, "Destination node required")) {
		return;
	}
	if (ensure_error(dest->host != NULL, "Destination host required")) {
		return;
	}
	if (ensure_error(dest->port != 0, "Destination port required")) {
		return;
	}

	// Complete the message with the sender's information
	node_copy(&message->sender, &me);

	// find the correct socket to send
	int sock = find_connection(dest);
	if (sock != -1) {
		if (send_message_internal(sock, message, message_size) != -1) {
			return;
		} else {
			log_error("failed first time");
		}
	} else {
		log_warning("sock == -1 for %s:%d", dest->host, dest->port);
	}

	// new connection if not found or if fail to send
	// Create a socket
	const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("socket");
		return;
	}

	log_info("open sock %d at addr %p", sockfd, &sockfd);

	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(dest->port);
	if (inet_pton(AF_INET, dest->host, &serv_addr.sin_addr) <= 0) {
		printf("Invalid address or Address not supported\n");
		return;
	}

	// Attempt to connect to the server
	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) ==
	    -1) {
		perror("connect");
		close(sockfd);
		return;
	}

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = sockfd;

	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev) == -1) {
		perror("epollctl");
		close(sockfd);
		return;
	}

	if (sock == -1) {
		log_info("add sock %d at addr %p", sockfd, &sockfd);
		sock = add_connection(*dest, sockfd);
		if (sock != sockfd) {
			close(sockfd);
		}
	} else {
		log_info("replace sock %d at addr %p", sockfd, &sockfd);
		sock = replace_socket(dest, sockfd);
		if (sock != sockfd) {
			close(sockfd);
		}
	}

	log_info("retry send message on sock %d", sock);

	if (send_message_internal(sock, message, message_size) == -1) {
		log_error("send message failed");
	}
}

void send_wait_message_nolock(const struct node_id *dest,
			      struct message *message, size_t message_size,
			      struct cond_var *cond_struct)
{
	send_message(dest, message, message_size);
	while (!cond_struct->predicate) {
		pthread_cond_wait(&cond_struct->cond, &cond_struct->lock);
	}
	cond_struct->predicate = false;
}

void send_wait_message(const struct node_id *dest, struct message *message,
		       size_t message_size, struct cond_var *cond_struct)
{
	pthread_mutex_lock(&cond_struct->lock);
	send_wait_message_nolock(dest, message, message_size, cond_struct);
	pthread_mutex_unlock(&cond_struct->lock);
}

void addHandler(const size_t message_type, struct node_id *sender,
		void callBack(struct message *message))
{
	pthread_mutex_lock(&callbacks_lock);
	callbacks[message_type] = callBack;
	pthread_mutex_unlock(&callbacks_lock);
}
