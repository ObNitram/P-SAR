#include "message.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct node_list node_list;
unsigned int nb_nodees;

void free_message(struct message *message)
{
	if (message != NULL) {
		// if (message->message_data != NULL) {
		// 	free(message->message_data);
		// }
		free(message);
	}
}

struct message *copy_message(const struct message *message, const size_t message_size)
{
	struct message *copy = malloc(message_size);
	memcpy(copy, message, message_size);
	return copy;
}