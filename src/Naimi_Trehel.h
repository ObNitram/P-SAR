#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>

#include "utils/utils.h"
#include "network/message.h"
#include "network/network.h"

/* 
    if you want to setup the debug mode, you have to ' export NT_DEBUG '
    to disable it just 'unset NT_DEBUG'
*/
#ifdef NT_DEBUG
    #include "../utils/logger.h"
    #define LOG_NT(fmt, ...) log_info(fmt, ##__VA_ARGS__)
    #define ENSURE_ERROR_NT(condition, fmt, ...) ensure_error(condition, fmt, ##__VA_ARGS__)
    #define ENSURE_WARNING_NT(condition, fmt, ...) ensure_warning(condition, fmt, ##__VA_ARGS__)
#else
    #define LOG_NT(fmt, ...)
    #define ENSURE_ERROR_NT(condition, fmt, ...) 0
    #define ENSURE_WARNING_NT(condition, fmt, ...) 0
#endif

extern bool token;
extern bool requesting;
extern struct node_id father;
extern struct node_id next;


extern void request_CS();

extern void release_CS();

extern void init_CS(struct node_id *father, bool token);