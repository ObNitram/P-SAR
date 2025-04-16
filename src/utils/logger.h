#pragma once

#include <stdio.h>
#include <time.h>
#include <unistd.h>


/// @brief Global log stream variable.
/// @details This variable defines the output stream for the logging messages.
/// It can be set to any valid FILE pointer (e.g., stdout, stderr, or a file opened with fopen).
/// By default, the stream is set to stdout.
extern FILE *g_log_stream;

/// @brief Macro to initialize the logger and log the initialization and output stream definition.
#define init_logger(stream) do {                                                       \
    g_log_stream = stream;			                                            \
    if ((stream) == stdout) {                                                            \
        log_info("Logger initialized on stdout.");                                   \
    } else if ((stream) == stderr) {                                                     \
        log_info("Logger initialized on stderr.");                                   \
    } else {                                                                             \
        log_info("Logger initialized on %p", (void*)(stream));                        \
    }                                                                                    \
} while (0)


/// @brief Internal function that logs a message with detailed context information.
/// @details The log message is formatted as follows:
/// [<TIME>] [PID: <pid>] [<LEVEL>] <File>:<Function>:<Line> - <Message>
/// where:
///   - <TIME>: Current Unix timestamp as an integer.
///   - <pid>: Process identifier of the calling process.
///   - <LEVEL>: Logging level (e.g., "INFO", "DEBUG", "ERROR").
///   - <File>: Name of the source file.
///   - <Function>: Name of the function.
///   - <Line>: Line number in the source file.
///   - <Message>: The log message to be recorded.
/// @param level The logging level (e.g., "INFO", "DEBUG", "ERROR").
/// @param message The formatted log message to log.
/// @param file The source file name.
/// @param function The function name.
/// @param line The line number.
extern void log_message_internal(const char *level, const char *message, const char *file,
                          const char *function, const int line);

#ifndef IGNORE

/// @brief Variadic macro wrapper for log_message_internal to automatically include file, function, and line information.
/// @param level The logging level.
/// @param fmt The format string for the log message.
/// @param ... The variadic arguments to format the message.
#define log_message(level, fmt, ...) do {                                              \
    char __log_buffer[1024];                                                           \
    /* Format the message using snprintf with provided arguments */                    \
    snprintf(__log_buffer, sizeof(__log_buffer), fmt, ##__VA_ARGS__);                    \
    log_message_internal(level, __log_buffer, __FILE__, __FUNCTION__, __LINE__);         \
} while(0)

/// @brief Macro for logging debug messages.
/// @param fmt The format string for the debug message.
/// @param ... The variadic arguments to format the message.
#define log_debug(fmt, ...)   log_message("DEBUG", fmt, ##__VA_ARGS__)

/// @brief Macro for logging info messages.
/// @param fmt The format string for the info message.
/// @param ... The variadic arguments to format the message.
#define log_info(fmt, ...)    log_message("INFO", fmt, ##__VA_ARGS__)

/// @brief Macro for logging warning messages.
/// @param fmt The format string for the warning message.
/// @param ... The variadic arguments to format the message.
#define log_warning(fmt, ...) log_message("WARNING", fmt, ##__VA_ARGS__)

/// @brief Macro for logging error messages.
/// @param fmt The format string for the error message.
/// @param ... The variadic arguments to format the message.
#define log_error(fmt, ...)   log_message("ERROR", fmt, ##__VA_ARGS__)

#else
    // Si IGNORE est défini, les macros ne font rien
    #define log_message(level, fmt, ...)  ((void)0)
    #define log_debug(fmt, ...)   ((void)0)
    #define log_info(fmt, ...)    ((void)0)
    #define log_warning(fmt, ...) ((void)0)
    #define log_error(fmt, ...)   ((void)0)
#endif

/// @brief Variadic macro to ensure a condition is true.
/// @details Checks the given condition, and if it evaluates to false, logs the provided formatted message
/// using the specified logging level. You may extend this macro to take additional actions (like exiting the program) if needed.
/// @param condition The condition to evaluate.
/// @param level The logging level to use when logging the message.
/// @param fmt The format string for the log message.
/// @param ... The variadic arguments to format the message.
#define ensure(condition, level, fmt, ...)                             \
(__extension__ ({                                                   \
	int __result = (condition);  /* Evaluate condition once */     \
	if (!__result) {                                                 \
		log_message(level, fmt, ##__VA_ARGS__);  /* Log if false */   \
	}                                                                \
	!__result;  /* Return the result of the condition */             \
}))

/// @brief Macro variant to ensure a condition is true.
/// @details If the condition is false, it logs the provided formatted message with a WARNING level.
/// @param condition The condition to evaluate.
/// @param fmt The format string for the log message.
/// @param ... The variadic arguments to format the message.
#define ensure_warning(condition, fmt, ...) ensure(condition, "WARNING", fmt, ##__VA_ARGS__)

/// @brief Macro variant to ensure a condition is true.
/// @details If the condition is false, it logs the provided formatted message with an ERROR level.
/// @param condition The condition to evaluate.
/// @param fmt The format string for the log message.
/// @param ... The variadic arguments to format the message.
#define ensure_error(condition, fmt, ...) ensure(condition, "ERROR", fmt, ##__VA_ARGS__)
