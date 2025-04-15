#include "network/message.h"
#include "utils/logger.h"
#include <stddef.h>

struct A {
	struct message a;
	struct node_id* b;
	size_t c;
};

int main(int argc, char **argv)
{
	init_logger(stderr);

	log_info("size of struct A (%lu,%lu)", sizeof(struct A), sizeof(struct message) + sizeof(struct node_id*) + sizeof(size_t));

	log_info("The answer is %d", 42);

	ensure_error(1>2, "This is an %s", "error");
}
