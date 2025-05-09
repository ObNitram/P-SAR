//this header is not supposed to be include elsewhere this purpose is for helper structure and the associated function
//This function doesnt depends on ANY global state and it for that it can be placed in a header...
#pragma once

#include "lock.h"
#include "utils/list.h"
#include "utils/counter_cond_var.h"
#include "network/utils/node_id.h"
#include "../network/utils/message_type.h"
#include <stddef.h>
#include <string.h>

/// @brief Structure representing a message for the slsm algorithm
/// @details Contains the type of the message, the identifier of the sender, the page, the mode and the initiator.
struct slsm_message {
	size_t page;
	enum lock_type mode;
	struct node_id initiator;
};
#define SLSM_SIZE sizeof(size_t) + sizeof(enum lock_type) + NODEID_SIZE

/// @brief
/// @details
/// @param msgt
/// @param sender
/// @param m
/// @param pid
/// @param cond
static void send_slsm_message(const enum message_type msgt,
			      const struct node_id *sender,
			      const enum lock_type m, size_t pid,
			      struct cond_var *cond)
{
	struct slsm_message request = {
		.page = pid,
		.mode = m,
		.initiator = me,
	};

	if (cond == NULL) {
		send_message1(msgt, sender, &request,
			      sizeof(struct slsm_message));
	} else {
		// send_wait_message_nolock(sender, (struct message *)&request,
		// 			 sizeof(struct slsm_message), cond);
		send_message1(msgt, sender, &request,
			      sizeof(struct slsm_message));
		wait_on_cond(cond, true);
	}
}

/// @brief
/// @details
struct delegate_message {
	struct node_id delegate;
};
#define DELEGATE_MESSAGE_SIZE NODEID_SIZE

void serialize_delegate_message(void *dest, const struct delegate_message *src)
{
	serialize_nodeid((char *)dest, &src->delegate);
}

struct delegate_message unserialize_delegate_message(void *src)
{
	struct delegate_message msg = { 0 };
	unserialize_nodeid(&msg.delegate, src);
	return msg;
}

/// @brief
/// @details
struct request {
	enum lock_type mode;
	struct node_id who;
	struct list_head next;
};
#define REQUEST_SIZE sizeof(enum lock_type) + NODEID_SIZE

//copy dest to src return the next adress
/// @brief
/// @details
static void *serialize_request(void *dest, const struct request *src)
{
	char *cursor = (char *)dest;
	memcpy(cursor, &src->mode, sizeof(src->mode));
	cursor += sizeof(src->mode);
	return serialize_nodeid(cursor, &src->who);
}

//copy dest to src return the next adress
/// @brief
/// @details
static void *unserialize_request(struct request *dest, void *src)
{
	char *cursor = (char *)src;
	memcpy(&dest->mode, cursor, sizeof(dest->mode));
	cursor += sizeof(dest->mode);
	return unserialize_nodeid(&dest->who, cursor);
}