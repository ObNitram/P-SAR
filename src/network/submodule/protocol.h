#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "utils/cond_var.h"
#include "utils/counter_cond_var.h"
#include "network/utils/node_id.h"

int init_network(const struct node_id *me, const unsigned int msg_type_number);

int exit_network(void);

int add_net_handler(const unsigned int type,
		    void (*callBack)(struct node_id *, void *));

int send_message1(const unsigned int type, const struct node_id *target,
		  const void *payload, const size_t size);

int broadcast_message1(const unsigned int type, const struct node_id **except,
		       const void *payload, const size_t size);

int multicast_message(const unsigned int type, struct node_id **targets,
		      const void *payload, const size_t size);

struct node_id get_info(void);

struct node_id *get_network(unsigned int *size);

int get_network_size(void);

int remove_from_network(const struct node_id *leaving_node);

int add_to_network(const struct node_id *add);