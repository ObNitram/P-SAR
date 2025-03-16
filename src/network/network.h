#pragma once

#include <stddef.h>
#include "../utils/list.h"

#define MAX_MESSAGES 100

/// @brief Structure representing a message.
/// @details Contains the type, size, and data of a message.
struct message {
	size_t message_type; ///< The type of the message.
	size_t message_size; ///< The size of the message data in bytes.
	void *message_data; ///< Pointer to the message data.
};

void free_message(struct message *message);

struct message *copy_message(const struct message *message);


/// @brief Structure representing a node identifier.
/// @details Contains the host and port information for a node.
struct node_id {
	char *host; ///< Hostname or IP address of the node.
	size_t port; ///< Port number of the node.
    int sock; ///< socket for read/write
    struct list_head list; ///< use this struct as a linked list
};


void start_server();

void stop_server();


/// @brief Sends a message to a destination node.
/// @details This function sends a message of a given type to the specified destination.
/// @param message_type The type of the message to send.
/// @param dest Pointer to the destination node identifier.
/// @param data Pointer to the data to be sent.
/// @param datasize Size of the data in bytes.
void send_message(size_t message_type,
                  const struct node_id *dest,
                  const void *data,
                  size_t datasize);


/// @brief Waits for a message of a specific type.
/// @details This function blocks until a message of the specified type is received.
///          If the sender parameter is NULL, it waits for a message from any sender.
/// @param message_type The type of message to wait for.
/// @param sender Pointer to the node identifier of the expected sender, or NULL to accept any sender.
/// @return A struct message containing the received message details.
struct message *wait_message(size_t message_type, struct node_id *sender);

/// @brief Adds a handler for messages of a specific type.
/// @details Registers a callback function that will be invoked when a message of the specified type is received.
///          If the sender parameter is NULL, the handler will be triggered for messages from any sender.
/// @param message_type The type of message for which the handler is registered.
/// @param sender Pointer to the node identifier of the sender to filter on, or NULL for any sender.
/// @param callBack The callback function to be invoked when the message is received.
void addHandler(size_t message_type,
                struct node_id *sender,
                void callBack(struct message *message));

/// @brief Deletes a handler for messages of a specific type.
/// @details Unregisters a previously added callback handler for a given message type.
///          If the sender parameter is NULL, the handler will be removed for messages from any sender.
/// @param message_type The type of message for which the handler is to be deleted.
/// @param sender Pointer to the node identifier of the sender to filter on, or NULL for any sender.
/// @param callBack The callback function to be removed.
void deleteHandler(size_t message_type,
                   struct node_id *sender,
                   void callBack(struct message *message));