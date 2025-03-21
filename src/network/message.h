#pragma once

#include <stddef.h>
#include <netinet/in.h>
#include "../utils/list.h"

#define MAX_MESSAGES 100

/// @brief Structure representing a node identifier.
/// @details Contains the host and port information for a node.
struct node_id {
	char host[INET6_ADDRSTRLEN]; ///< Hostname or IP address of the node.
	int port; ///< Port number of the node.
};

/// @brief Structure representing a message.
/// @details Contains the type of the message and the identifier of the sender.
struct message {
	size_t message_type; ///< The type of the message.
	struct node_id sender; ///< The sender of the message.
};

struct INFO_DSM_message {
	struct message header;
	unsigned int nb_pages;
	unsigned int nb_nodes;
};

/// @brief Frees a dynamically allocated message.
/// @details This function releases the memory allocated for a message, helping to prevent memory leaks.
///          The provided pointer must refer to a message that was allocated dynamically.
/// @param message Pointer to the message to be freed.
void free_message(struct message *message);

/// @brief Creates a copy of a message.
/// @details This function allocates memory for a new message and copies the content of the source message
///          into it. The total size of the message (in bytes) must be provided to ensure that the entire
///          message is copied correctly, especially if the message includes additional data beyond the basic structure.
/// @param message Pointer to the source message to copy.
/// @param message_size Total size of the message to copy (in bytes).
/// @return Pointer to the newly allocated copy of the message, or NULL if memory allocation fails.
struct message *copy_message(const struct message *message,
                             const size_t message_size);