#pragma once
#include <stdio.h>
#include <time.h>
#include <unistd.h>

/*
@brief Global log stream variable.
@details This variable defines the output stream for the logging messages.
         It can be set to any valid FILE pointer (e.g., stdout, stderr, or a file opened with fopen).
         By default, the stream is set to stdout.
*/
FILE *g_log_stream;

/*
@brief Internal function that logs a message with detailed context information.
@details The log message is formatted as follows:
         [<TIME>] [PID: <pid>] [<LEVEL>] <File>:<Function>:<Line> - <Message>
         where:
           - <TIME>: Current Unix timestamp as an integer.
           - <pid>: Process identifier of the calling process.
           - <LEVEL>: Logging level (e.g., "INFO", "DEBUG", "ERROR").
           - <File>: Name of the source file.
           - <Function>: Name of the function.
           - <Line>: Line number in the source file.
           - <Message>: The log message to be recorded.
@params level The logging level (e.g., "INFO", "DEBUG", "ERROR").
@params message The message to log.
@params file The source file name.
@params function The function name.
@params line The line number.
*/
void log_message_internal(char *level, char *message, const char *file,
                                 const char *function, int line)
{
	// Get the current time as a Unix timestamp (seconds since the epoch)
	time_t now = time(NULL);

	// Get the process ID
	pid_t pid = getpid();

	// Print the formatted log message to the global log stream.
	fprintf(g_log_stream, "[%ld] [PID: %d] [%s] %s:%s:%d - %s\n",
	        now, pid, level, file, function, line, message);
}

/*
@brief Macro wrapper for log_message_internal to automatically include file, function, and line information.
@params level The logging level.
@params message The message to log.
*/
#define log_message(level, message) log_message_internal(level, message, __FILE__, __FUNCTION__, __LINE__)

/*
@brief Macro for logging debug messages.
@params message The message to log.
*/
#define log_debug(message)   log_message("DEBUG", message)

/*
@brief Macro for logging info messages.
@params message The message to log.
*/
#define log_info(message)    log_message("INFO", message)

/*
@brief Macro for logging warning messages.
@params message The message to log.
*/
#define log_warning(message) log_message("WARNING", message)

/*
@brief Macro for logging error messages.
@params message The message to log.
*/
#define log_error(message)   log_message("ERROR", message)


/*
@brief Macro to ensure a condition is true.
@details Checks the given condition, and if it evaluates to false, logs the provided message
	 using the specified logging level.
	 You may extend this macro to take additional actions (like exiting the program) if needed.
@params condition The condition to evaluate.
@params message The message to log if the condition is false.
@params level The logging level to use when logging the message.
*/
#define ensure(condition, level, message)	\
do {						\
	if (!(condition)) {			\
		log_message(level, message);	\
	}					\
} while(0)

/*
@brief Macro variant to ensure a condition is true.
@details If the condition is false, it logs the provided message with a WARNING level.
@params condition The condition to evaluate.
@params message The message to log if the condition is false.
*/
#define ensure_warning(condition, message) ensure(condition, "WARNING", message)

/*
@brief Macro variant to ensure a condition is true.
@details If the condition is false, it logs the provided message with an ERROR level.
@params condition The condition to evaluate.
@params message The message to log if the condition is false.
*/
#define ensure_error(condition, message) ensure(condition, "ERROR", message)