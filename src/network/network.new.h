#pragma once

#include "network/utils/node_id.h"

int join_network(const struct node_id *me, const struct node_id *father);

int leave_network(void);

int add_net_handler(const unsigned int type,
		    void (*callBack)(struct node_id *, void *));

int send_message1(const unsigned int type, const struct node_id *target,
		  const void *payload, const size_t size);

int broadcast_message1(const unsigned int type, const struct node_id **except,
		       const void *payload, const size_t size);

int multicast_message(const unsigned int type, struct node_id **targets,
		      const void *payload, const size_t size);

struct node_id *get_network(unsigned int *size);