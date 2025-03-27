#pragma once

#include "../network/message.h"
#include "../network/network.h"
#include <string.h>

const struct node_id EMPTY_NODE = {"", -1};
struct node_id me;

struct node_list {
	struct node_id node;
	struct list_head nlist;
};

extern struct node_list node_list;
extern unsigned int nb_nodees;

/// @brief Represents the type of message.
/// @details This enum defines the available message types.
enum message_type {
    ASK_LOCK,
    GET_LOCK,
    UNLOCK,
	JOIN_DSM, 
	DSM_INFO
};