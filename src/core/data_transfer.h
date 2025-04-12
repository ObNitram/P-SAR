#pragma once

#include "../network/network.h"
#include "../network/message.h"

/* 
    if you want to setup the debug mode, you have to ' export DATA_TRANS_DEBUG '
    to disable it just 'unset DATA_TRANS_DEBUG'
*/
#ifdef DATA_TRANS_DEBUG
    #include "../utils/logger.h"
    #define LOG_DATA_TRANS(fmt, ...) log_info(fmt, ##__VA_ARGS__)
    #define ENSURE_ERROR_DATA_TRANS(condition, fmt, ...) ensure_error(condition, fmr, ##__VA_ARGS__)
    #define ENSURE_WARNING_DATA_TRANS(condition, fmt, ...) ensure_warning(condition, fmr, ##__VA_ARGS__)
#else
    #define LOG_DATA_TRANS(fmt, ...)
    #define ENSURE_ERROR_DATA_TRANS(condition, fmt, ...) 0
    #define ENSURE_WARNING_DATA_TRANS(condition, fmt, ...) 0
#endif

extern struct node_id *page_owners;

extern void set_new_owner(size_t page_id, struct node_id *new_owner);

extern void sync_page(struct node_id *owner, size_t index);

extern void init_data_transfer(unsigned int nb_pages, struct node_id* owners);

extern void clean_data_transfer();