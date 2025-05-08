#include "logger.h"

FILE *g_log_stream = NULL;

void log_message_internal(const char *level, const char *message,
			  const char *file, const char *function,
			  const int line)
{
	//default on standard output
	if (g_log_stream == NULL) {
		g_log_stream = stdout;
	}

	// Get the current time as a Unix timestamp (seconds since the epoch)
	time_t now = time(NULL);

	// Get the process ID
	pid_t pid = getpid();

	// Print the formatted log message to the global log stream.
	fprintf(g_log_stream, "[%ld] [PID: %d] [%s] %s:%s:%d - %s\n", now, pid,
		level, file, function, line, message);
}