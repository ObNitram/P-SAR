#include <signal.h>


#include "utils/logger.h"

#define DEBUG_BREAK() __asm__ volatile("int $3")

int main(int argc, char **argv)
{
	// Init the logger systeme
	g_log_stream = stdout;
	ensure_error(0, "message");

	printf("Usage: %s message\n", argv[0]);
}