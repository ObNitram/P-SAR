#include <stdlib.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>

#include "comm/comm.h"
#include "protocol.h"
#include "utils/list.h"
#include "network/utils/cleanup.h"
#include "network/utils/network_header.h"
#include "network/primitive_sock.h"

#define ERROR_LOG
#include "utils/logger.h"

#define MAX_PENDING_CONNEXION 5

static void handle_server_socket(int fd);
static void handle_client_socket(int fd);

// ==================== MANAGE CONNEXION ====================

/// @brief structure representing a connexion for the TCP module
struct connexion {
	struct node_id info;
	pthread_mutex_t handler_mutex;
	pthread_cond_t handler_cond;
	struct list_head list;
	int fd;
	int ticket;
	int counter;
	bool temporary;
	bool deleted;
};

/// @brief function to get a connexion from a list
/// @param list the list where to search
/// @param info the node info of the connexion to search
/// @return the connexion found or NULL if not found
static struct connexion *get_connexion(struct list_head *list,
				       const struct node_id *info,
				       const bool istemporary)
{
	struct connexion *cur;
	list_for_each_entry(cur, list, list) {
		if (node_equal(&cur->info, info) &&
		    (istemporary || !cur->temporary)) {
			return cur;
		}
	}
	return NULL;
}

/// @brief function to create a connexion
/// @param info the node info associated to the new connexion
/// @param socket the socket to communicate to the node
/// @return a new connexion reserve with malloc initialized as temporary connexion
static struct connexion *create_connexion(const struct node_id *info,
					  const int socket)
{
	struct connexion *new_connexion =
		(struct connexion *)malloc(sizeof(struct connexion));
	if (new_connexion == NULL) {
		log_error("fail to create new connexion: %s", strerror(errno));
		return NULL;
	}

	pthread_cond_init(&new_connexion->handler_cond, NULL);
	pthread_mutex_init(&new_connexion->handler_mutex, NULL);
	new_connexion->info = *info;
	new_connexion->fd = socket;
	new_connexion->counter = 0;
	new_connexion->ticket = 0;
	new_connexion->temporary = true;
	new_connexion->deleted = false;
	return new_connexion;
}

// if a connexion is in a list it must be remove before calling it and not referenced in another context
static void destroy_connexion(struct connexion *connexion)
{
	pthread_mutex_lock(&connexion->handler_mutex);
	// flag the connexion to deleted
	connexion->deleted = true;

	// wait all thread to terminate
	while (connexion->counter != connexion->ticket) {
		log_debug("wait all thread to finish");
		pthread_cond_signal(&connexion->handler_cond);
		pthread_cond_wait(&connexion->handler_cond,
				  &connexion->handler_mutex);
	}

	pthread_mutex_unlock(&connexion->handler_mutex);

	// free the connexion
	free(connexion);
}

static void destroy_connexions(struct list_head *list)
{
	struct connexion *cur, *tmp;
	list_for_each_entry_safe(cur, tmp, list, list) {
		list_del(&cur->list);
		destroy_connexion(cur);
	}
}

// ==================== MANAGE CONNEXION ====================

/// @brief structure representing the handler thread argument
struct handler_args {
	struct header header;
	struct connexion *connexion;
	int ticket;
	char payload[];
};

/// @brief a structure representing the context of this module
static struct {
	pthread_mutex_t context_mutex;
	pthread_mutex_t callbacks_mutex;
	struct node_id info;
	void (**callbacks)(struct node_id *, void *);
	struct list_head network;
	int network_size;
	unsigned int max_message_type;
} context = { .context_mutex = PTHREAD_MUTEX_INITIALIZER,
	      .callbacks_mutex = PTHREAD_MUTEX_INITIALIZER,
	      .info = EMPTY_NODE_INITIALIZER,
	      .callbacks = NULL,
	      .network_size = 0,
	      .max_message_type = 0 };

/// @brief function to check socket validity
static bool sockisinvalid(int sock)
{
	return fcntl(sock, F_GETFL) < 0 && errno == EBADF;
}

/// @brief function to create a server socket
/// @param sockinfo the ip and port where the socket need to listen
/// @return 0 on success, -1 on error
static int create_server_socket(const struct node_id *sockinfo)
{
	const int listen_sock = socket(AF_INET, SOCK_STREAM, 0);

	if (listen_sock < 0) {
		log_error("fail to open server socket: %s", strerror(errno));
		return -1;
	}

	struct sockaddr_in serveraddr = { 0 };
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(sockinfo->port);

	if (inet_pton(AF_INET, sockinfo->host, &serveraddr.sin_addr) <= 0) {
		log_error("fail to get IP adress: %s", strerror(errno));
		close(listen_sock);
		return -1;
	}

	// Enable address reuse.
	const int optval = 1;
	if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &optval,
		       sizeof(optval)) < 0) {
		log_error("fail to set option SO_REUSEADDR: %s",
			  strerror(errno));
	}

	if (bind(listen_sock, (struct sockaddr *)&serveraddr,
		 sizeof(serveraddr)) < 0) {
		log_error("fail bind server socket: %s", strerror(errno));
		close(listen_sock);
		return -1;
	}

	// Start listening for incoming connexions.
	if (listen(listen_sock, MAX_PENDING_CONNEXION) < 0) {
		log_error("fail to start listening: %s", strerror(errno));
		close(listen_sock);
		return -1;
	}

	if (add_handler(handle_server_socket, listen_sock) != 0) {
		log_error("fail to manage server socket fd=%d", listen_sock);
		close(listen_sock);
		return -1;
	}

	return 0;
}

/// @brief function to create a client socket connected to the given target
/// @param target the server socket to connect to
/// @return 0 on success or -1 on error
static int create_client_socket(const struct node_id *target)
{
	const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		log_error("fail to open client socket: %s", strerror(errno));
		return -1;
	}

	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(target->port);

	if (inet_pton(AF_INET, target->host, &serv_addr.sin_addr) <= 0) {
		log_error("fail to get server IP: %s", strerror(errno));
		return -1;
	}

	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) ==
	    -1) {
		log_error("fail to connect to server: %s", strerror(errno));
		close(sockfd);
		return -1;
	}

	if (add_handler(handle_client_socket, sockfd) != 0) {
		log_error("fail to manage client socket fd=%d", sockfd);
		close(sockfd);
		return -1;
	}

	return sockfd;
}

/// @brief function to handle new connexion
/// @param fd the file descripteur of the server socket
static void handle_server_socket(int fd)
{
	const int clientsock = accept(fd, NULL, NULL);

	if (clientsock < 0) {
		log_error("accept failed: %s", strerror(errno));
		// try to re-establish server socket (maybe useless ??)
		if (delete_handler(fd) != 0 ||
		    create_server_socket(&context.info) != 0) {
			log_error("server socket cant be retablish");
			exit_network();
		}
	} else {
		//add socket to comm loop
		add_handler(handle_client_socket, clientsock);
	}
}

/// @brief function called by the handler of client connexion to execute a handler on a message of this connexion, DO NOT CALL context here !!
/// all data need to be copy in handle_client_socket
/// the function lock the given connexion
/// @param data argument of the thread embedded in a struct handler_args
/// @return NULL
static void *exec_handler(void *data)
{
	defer(cleanup_free) struct handler_args *args = data;

	pthread_mutex_lock(&args->connexion->handler_mutex);
	defer_unlock_mutex(&args->connexion->handler_mutex);

	log_debug("execute handler");

	//wait previous message handler of this connexion to be executed
	while (args->connexion->counter != args->ticket &&
	       args->connexion->deleted == false) {
		log_debug("wait previous handler");
		pthread_cond_wait(&args->connexion->handler_cond,
				  &args->connexion->handler_mutex);
	}

	//if main thread want to delete the connexion no handler is executed
	if (args->connexion->deleted) {
		log_warning("connexion deleted");
		args->connexion->counter++;
		pthread_cond_signal(&args->connexion->handler_cond);
		return NULL;
	}

	//get handler
	pthread_mutex_lock(&context.callbacks_mutex);
	void (*handler)(struct node_id *, void *) =
		context.callbacks ?
			context.callbacks[args->header.message_type] :
			NULL;
	pthread_mutex_unlock(&context.callbacks_mutex);

	//execute handler out of context lock
	if (handler != NULL)
		handler(&args->header.sender, args->payload);

	//notify next handler
	args->connexion->counter++;
	pthread_cond_broadcast(&args->connexion->handler_cond);

	return NULL;
}

/// @brief function to handle a client connexion
/// @details This function is an entry point of the module called by the main loop event, context is lock at first and NOT UNLOCK UNTIL THE END.
/// First, read the message header, next the patload size, and finish with the payload.
/// Second, find the connexion associated with the sender of the message or create it if new client.
/// And execute the corresponding handler if register.
/// @param fd the file descriptor of the client socket
static void handle_client_socket(int fd)
{
	char buff[HEADER_SIZE] = { 0 };
	struct header header = { 0 };
	size_t size = 0;

	pthread_mutex_lock(&context.context_mutex);
	defer_unlock_mutex(&context.context_mutex);

	if (node_isempty(&context.info)) {
		log_error("server is stop");
		return;
	}

	int err;
	// receive header of the message
	if ((err = Recv_all(fd, buff, sizeof(buff))) < sizeof(buff)) {
		if (delete_handler(fd) != 0) {
			log_error("fail to delete client handler");
		}
		return;
	}

	unserialize_header(&header, buff);

	log_debug("receive message from %s:%d", header.sender.host,
		  header.sender.port);

	//receive payload size
	if (Recv_all(fd, &size, sizeof(size)) < sizeof(size)) {
		log_error("fail to receive payload size: %s", strerror(errno));
		if (delete_handler(fd) != 0) {
			log_error("fail to delete client handler");
		}
		return;
	}

	struct handler_args *args = malloc(sizeof(struct handler_args) + size);
	if (args == NULL) {
		log_error("fail to create payload: %s", strerror(errno));
		Clean_all(fd, size);
		return;
	}

	//receive payload
	if (size != 0 && Recv_all(fd, args->payload, size) < size) {
		log_error("fail to receive payload: %s", strerror(errno));
		if (delete_handler(fd) != 0) {
			log_error("fail to delete client handler");
		}
		free(args);
		return;
	}

	//get connexion
	struct connexion *sender_co =
		get_connexion(&context.network, &header.sender, true);
	if (sender_co == NULL) {
		sender_co = create_connexion(&header.sender, fd);
		if (sender_co == NULL) {
			log_error("fail to add %s:%d to network using fd = %d",
				  header.sender.host, header.sender.port, fd);
			free(args);
			return;
		}
		list_add(&sender_co->list, &context.network);
	}

	//check message type
	if (header.message_type >= context.max_message_type) {
		log_error("message type out of bound");
		free(args);
		return;
	}

	//fill handler args
	args->ticket = sender_co->ticket++;
	args->connexion = sender_co;
	args->header = header;

	//execute corresponding callback
	pthread_t handler;
	if (pthread_create(&handler, NULL, exec_handler, (void *)args) != 0) {
		log_error("fail to create thread: %s", strerror(errno));
		free(args);
		return;
	}
	if (pthread_detach(handler) != 0) {
		log_error("fail to detach thread: %s", strerror(errno));
		//kill thread after timeout ??
	}
}

/// @brief init the TCP module to manage tcp connection
/// @param info information for the server socket
/// @param msg_type_number number of message type to handle
/// @return 0 on success, -1 on error
int init_network(const struct node_id *info, const unsigned int max_msg_type)
{
	pthread_mutex_lock(&context.context_mutex);
	defer_unlock_mutex(&context.context_mutex);

	if (!node_isempty(&context.info)) {
		log_error("server already running");
		return -1;
	}

	//create server socket
	if (create_server_socket(info) != 0) {
		log_error("fail to add server socket");
		return -1;
	}

	//reserve callback
	context.callbacks = calloc(max_msg_type,
				   sizeof(void (*)(struct node_id *, void *)));
	if (context.callbacks == NULL) {
		log_error("fail to reserve callback : %s", strerror(errno));
		return -1;
	}

	//set context
	context.info = *info;
	context.network_size = 0;
	context.max_message_type = max_msg_type;
	INIT_LIST_HEAD(&context.network);

	log_debug("init network with %s:%d", info->host, info->port);

	return 0;
}

/// @brief exit the tcp module
/// @return 0 on success or -1 on error
int exit_network(void)
{
	pthread_mutex_lock(&context.context_mutex);
	defer_unlock_mutex(&context.context_mutex);

	if (node_isempty(&context.info)) {
		log_error("server already stop");
		return -1;
	}

	//clean handler
	delete_handlers(handle_client_socket);
	delete_handlers(handle_server_socket);

	//destroy all connexions
	destroy_connexions(&context.network);

	//free callbacks
	free(context.callbacks);

	//reset context
	context.max_message_type = 0;
	context.callbacks = NULL;
	context.info = EMPTY_NODE;
	context.network_size = 0;
	INIT_LIST_HEAD(&context.network);

	return 0;
}

/// @brief function to add a handler bind to a specific message type
/// @param type the message type
/// @param callback the callback to call for each reception of this message type
/// @return 0 on success or -1 on error
int add_net_handler(const unsigned int type,
		    void (*callback)(struct node_id *, void *))
{
	// protect under callback mutex not the context one
	pthread_mutex_lock(&context.callbacks_mutex);
	defer_unlock_mutex(&context.callbacks_mutex);

	if (node_isempty(&context.info)) {
		log_error("network is not initialized");
		return -1;
	}

	if (type >= context.max_message_type) {
		log_error("incompatible type : type(%d) >= max(%d)", type,
			  context.max_message_type);
		return -1;
	}

	context.callbacks[type] = callback;

	return 0;
}

/// @brief function to add node to network
/// @details This function is the unique way to make a permanent connexion
/// It make the existing connexion permanent or create it and tag it permanent
/// @param add the node to add to network
/// @return 0 on success or -1 on error
int add_to_network(const struct node_id *add)
{
	pthread_mutex_lock(&context.context_mutex);
	defer_unlock_mutex(&context.context_mutex);

	// get or create connexion
	struct connexion *find = get_connexion(&context.network, add, true);
	if (find == NULL) {
		//create client socket
		const int clientsock = create_client_socket(add);
		if (clientsock < 0) {
			log_error("fail to create client socket");
			return -1;
		}

		if ((find = create_connexion(add, clientsock)) == NULL) {
			log_error("fail to create new connexion");
			return -1;
		}
		list_add(&find->list, &context.network);
	}

	// make the connexion permanent
	find->temporary = false;

	return 0;
}

/// @brief function to remove a node from network the only way for user to delete a node from network
/// @param leaving_node the node to remove from network
/// @return 0 on success; -1 on error
int remove_from_network(const struct node_id *leaving_node)
{
	pthread_mutex_lock(&context.context_mutex);
	defer_unlock_mutex(&context.context_mutex);

	log_debug("remove %s:%d from network", leaving_node->host,
		  leaving_node->port);

	//search for connexion
	struct connexion *cur, *tmp;
	list_for_each_entry_safe(cur, tmp, &context.network, list) {
		if (node_equal(&cur->info, leaving_node)) {
			list_del(&cur->list);
			destroy_connexion(cur);
			return 0;
		}
	}

	log_warning("given node not found");

	return -1;
}

/// @brief internal function to send a message on a connexion
/// @details This function send a message on a valid and permanent connexion.
/// It need to be protect under the context mutex because of manipulation of the context
/// @param type the type of the message to send
/// @param connexion the connexion where to send the message
/// @param payload the data to send
/// @param size the size of the data to send
/// @return 0 on success or -1 on error
static int __send_message(const unsigned int type, struct connexion *connexion,
			  const void *payload, const size_t size)
{
	// check if connexion has a valid socket
	if (sockisinvalid(connexion->fd)) {
		log_warning("invalid socket detected");
		if (delete_handler(connexion->fd) != 0) {
			log_error("fail to remove handler");
		}

		int targetfd = create_client_socket(&connexion->info);
		if (targetfd == -1) {
			log_error("cannot open client socket");
			list_del(&connexion->list);
			destroy_connexion(connexion);
			return -1;
		}

		connexion->fd = targetfd;
	}

	//create header
	struct header header = { .message_type = type, .sender = context.info };
	char buffer[HEADER_SIZE] = { 0 };
	serialize_header(buffer, &header);

	//send header
	if (send_all(connexion->fd, buffer, sizeof(buffer)) != 0) {
		log_error("fail to send header: %s", strerror(errno));
		return -1;
	}

	//send payload size
	if (send_all(connexion->fd, (const char *)&size, sizeof(size_t)) != 0) {
		log_error("fail to send size: %s", strerror(errno));
		return -1;
	}

	//send payload
	if (send_all(connexion->fd, payload, size) != 0) {
		log_error("fail to send payload: %s", strerror(errno));
		return -1;
	}
	return 0;
}

/// @brief function to send a message to a target node
/// @param type the type of the message to send
/// @param target the node to send the message
/// @param payload the data to send
/// @param size the size of the data to send
/// @return 0 on success and -1 on error
int send_message1(const unsigned int type, const struct node_id *target,
		  const void *payload, const size_t size)
{
	pthread_mutex_lock(&context.context_mutex);
	defer_unlock_mutex(&context.context_mutex);

	if (node_isempty(&context.info)) {
		log_error("network is not initialized");
		return -1;
	}

	//get only validate connexion
	struct connexion *find = get_connexion(&context.network, target, false);
	if (find == NULL) {
		log_error("(%s:%d) is not in network", target->host,
			  target->port);
		return -1;
	}

	//send message
	if (__send_message(type, find, payload, size) != 0) {
		log_error("fail to send message");
		return -1;
	}

	return 0;
}

/// @brief function to broadcast a message in network.
/// @param type the type of the message to send
/// @param except a list of exception node
/// @param payload the data to send
/// @param size the size of the data to send
/// @return the number of message send or -1 on error.
int broadcast_message1(const unsigned int type, const struct node_id **except,
		       const void *payload, const size_t size)
{
	pthread_mutex_lock(&context.context_mutex);
	defer_unlock_mutex(&context.context_mutex);

	if (node_isempty(&context.info)) {
		log_error("network is not initialized");
		return -1;
	}

	int message_send = 0;

	//internal send message can destroy an invalid connexion, foreach need to be safe
	struct connexion *cur, *tmp;
	list_for_each_entry_safe(cur, tmp, &context.network, list) {
		// check if connexion not in except
		bool ignore = false;
		for (int i = 0; except != NULL && except[i] != NULL; i++) {
			if (node_equal(except[i], &cur->info)) {
				ignore = true;
				break;
			}
		}
		//if connexion is temporary or flag ignore continue
		if (cur->temporary || ignore)
			continue;

		if (__send_message(type, cur, payload, size) != 0) {
			log_error("fail to send message to %s:%d",
				  cur->info.host, cur->info.port);
		} else {
			message_send++;
		}
	}

	return message_send;
}

/// @brief function to get a list of node in network
/// @details This function return a list of all node with not a temporary connexion.
/// The list is allocate with malloc and need to be free after use.
/// @param size a pointer to an unsigned int to store the network size
/// @return a list reserve with malloc that store the nodes
struct node_id *get_network(unsigned int *restrict size)
{
	pthread_mutex_lock(&context.context_mutex);
	defer_unlock_mutex(&context.context_mutex);

	*size = 0;

	if (node_isempty(&context.info)) {
		log_error("network is not initialized");
		return NULL;
	}

	*size = 1;

	struct connexion *cur;
	//calculate network size
	list_for_each_entry(cur, &context.network, list) {
		if (cur->temporary == false) {
			(*size)++;
		}
	}

	//reserve buffer
	struct node_id *ret = malloc(sizeof(struct node_id) * (*size + 1));
	if (ret == NULL) {
		log_error("fail to create buffer: %s", strerror(errno));
		return NULL;
	}

	int i = 0;
	ret[i++] = context.info;

	//fill buffer
	list_for_each_entry(cur, &context.network, list) {
		if (cur->temporary == false) {
			ret[i++] = cur->info;
		}
	}

	return ret;
}

struct node_id get_info(void)
{
	pthread_mutex_lock(&context.context_mutex);
	defer_unlock_mutex(&context.context_mutex);

	return context.info;
}