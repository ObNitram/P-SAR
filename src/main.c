#include "utils/logger.h"

#define DEBUG_BREAK() __asm__ volatile("int $3")

int main(int argc, char **argv)
{
	init_logger(stderr);

	log_info("The answer is %d", 42);

	ensure_error(1>2, "This is an %s", "error");
}