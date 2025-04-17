#pragma once

#include "network/message.h"

extern void request_CS();

extern void release_CS();

void init_CS(const struct node_id *father_init, bool token_init,
             bool requesting_init);

extern void clear_CS();

extern int leave_CS();