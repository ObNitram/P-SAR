#pragma once

#include "../network/network.h"
#include "../network/message.h"

extern void sync_page(struct node_id *owner, size_t index);

extern void set_data_transfer_handler();