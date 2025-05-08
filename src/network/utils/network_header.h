#pragma once

#include "network/utils/node_id.h"
#include <stddef.h>

/// @brief Structure representing a message.
/// @details Contains the type of the message and the identifier of the sender.
struct header {
	size_t message_type; ///< The type of the message.
	struct node_id sender; ///< The sender of the message.
};
#define HEADER_SIZE (NODEID_SIZE + sizeof(size_t))

/// @brief Serialize a struct node_id
/// @details This function srialize a given struct node_id into the given buffer. The buffer size must be greater than NODE_SIZE
/// @param dest
/// @param src
/// @return The end of the buffer or NULL on failure
static void *serialize_header(void *dest, const struct header *src)
{
	char *cursor = (char *)dest;
	memcpy(cursor, &src->message_type, sizeof(src->message_type));
	cursor += sizeof(src->message_type);

	return serialize_nodeid(cursor, &src->sender);
}

/// @brief
/// @details
static void *unserialize_header(struct header *dest, void *src)
{
	char *cursor = (char *)src;
	memcpy(&dest->message_type, cursor, sizeof(dest->message_type));
	cursor += sizeof(dest->message_type);

	return unserialize_nodeid(&dest->sender, cursor);
}