#pragma once

#include "network/message.h"

extern void request_CS(void);

extern void release_CS(void);

extern void init_CS(const struct node_id *father_init, bool token_init,
		    bool requesting_init);

extern void clean_CS(void);

extern void leave_CS(const struct node_id new_root);