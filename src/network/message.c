#include "message.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void free_message(struct message *message)
{
	if (message != NULL) {
		free(message);
	}
}

struct message *copy_message(const struct message *message,
			     const size_t message_size)
{
	struct message *copy = malloc(message_size);
	memcpy(copy, message, message_size);
	return copy;
}