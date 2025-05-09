#pragma once

#include <netinet/in.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>

/// @brief Structure representing a node identifier.
/// @details Contains the host and port information for a node.
struct node_id {
	char host[INET6_ADDRSTRLEN]; ///< Hostname or IP address of the node.
	int port; ///< Port number of the node.
};
#define NODEID_SIZE ((sizeof(char) * INET6_ADDRSTRLEN) + sizeof(int))

#define EMPTY_NODE_INITIALIZER        \
	{                             \
		.host = 0, .port = -1 \
	}

static const struct node_id EMPTY_NODE = { .host = 0, .port = -1 };

/// @brief Serialize a struct node_id
/// @details This function srialize a given struct node_id into the given buffer. The buffer size must be greater than NODE_SIZE
/// @param dest
/// @param src
/// @return The end of the buffer or NULL on failure
static void *serialize_nodeid(void *dest, const struct node_id *src)
{
	char *cursor = (char *)dest;
	memcpy(cursor, src->host, sizeof(src->host));
	cursor += sizeof(src->host);
	memcpy(cursor, &src->port, sizeof(src->port));
	cursor += sizeof(src->port);
	return cursor;
}

/// @brief
/// @details
static void *unserialize_nodeid(struct node_id *dest, void *src)
{
	char *cursor = (char *)src;
	memcpy(dest->host, cursor, sizeof(dest->host));
	cursor += sizeof(dest->host);
	memcpy(&dest->port, cursor, sizeof(dest->port));
	cursor += sizeof(dest->port);
	return cursor;
}

/// @brief
/// @details
static bool node_equal(const struct node_id *node1, const struct node_id *node2)
{
	return (node1->port == node2->port) &&
	       (strcmp(node1->host, node2->host) == 0);
}

/// @brief Compares node1 with node2.
/// @return Return a positive value if node1 > node2 and a negative value if node1 < node2.
static int node_cmp(const struct node_id *node1, const struct node_id *node2)
{
	struct in_addr ip_addr1;
	struct in_addr ip_addr2;
	if (inet_pton(AF_INET, node1->host, &ip_addr1) <= 0 ||
	    inet_pton(AF_INET, node2->host, &ip_addr2) <= 0) {
		return 0;
	}

	if (ip_addr1.s_addr == ip_addr2.s_addr) {
		return node1->port - node2->port;
	} else {
		return ip_addr1.s_addr - ip_addr2.s_addr;
	}
}

static void node_empty(struct node_id *node)
{
	node->port = -1;
	memset(node->host, 0, INET6_ADDRSTRLEN);
}

static bool node_isempty(const struct node_id *node)
{
	return (node->port == -1) && (strlen(node->host) == 0);
}

/// @brief
/// @details
static void node_copy(struct node_id *dst, const struct node_id *src)
{
	memcpy(dst->host, src->host, INET6_ADDRSTRLEN * sizeof(char));
	dst->port = src->port;
}

//add function to print a node_id and use a more compact way to store it ??