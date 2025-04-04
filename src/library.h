#pragma once
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include "network/network.h"
#include "core/core.h"
#include "core/data_transfer.h"
#include "network/message.h"
#include "utils/utils.h"
#include "utils/logger.h"
#include "INFO_DSM_message.h"

/* 
    if you want to setup the debug mode, you have to ' export LIBRARY_DEBUG '
    to disable it just 'unset LIBRARY_DEBUG'
*/
#ifdef LIBRARY_DEBUG
    #include "../utils/logger.h"
    #define LOG_LIBRARY(fmt, ...) log_info(fmt, ##__VA_ARGS__)
    #define ENSURE_ERROR_LIBRARY(condition, fmt, ...) ensure_error(condition, fmr, ##__VA_ARGS__)
    #define ENSURE_WARNING_LIBRARY(condition, fmt, ...) ensure_warning(condition, fmr, ##__VA_ARGS__)
#else
    #define LOG_LIBRARY(fmt, ...)
    #define ENSURE_ERROR_LIBRARY(condition, fmt, ...) 0
    #define ENSURE_WARNING_LIBRARY(condition, fmt, ...) 0
#endif

/// @brief Initializes the distributed shared memory for the initial node with the specified size.
///
/// This function allocates and initializes a distributed shared memory region for the initial node.
///
/// @param size The size (in bytes) of the shared memory to initialize.
/// @param port The port you use to communicate
/// @return A pointer to the allocated shared memory region on success, NULL otherwise.
void *Init_DSM(size_t size, int port);

void free_DSM(void);


/// @brief Adds a new node to the distributed shared memory system by connecting to an existing node.
///
/// This function connects the new node to the distributed shared memory system by contacting
/// the node at the specified host address and returns the address of the shared memory region.
///
/// @param host The hostname or IP address of the existing node to connect to.
/// @param connect_port The port of the existing node to connect to.
/// @param server_port The port you listen to add a new node
/// @return A pointer to the shared memory region.
void *join_DSM(const char *host, int connect_port, int server_port);


/// @brief Requests a read lock for the specified memory region.
///
/// This function requests a read lock for the segment of memory of size `s` starting at address `adr`.
/// If one or more corresponding pages are write-locked by another node, this call blocks until the lock is available.
///
/// @param adr A pointer to the memory region to lock for reading.
/// @param s The size (in bytes) of the memory region to lock.
void lock_read(void *adr, size_t s);


/// @brief Releases the read lock for the specified memory region.
///
/// This function releases a previously acquired read lock for the segment of memory of size `s` starting at address `adr`.
///
/// @param adr A pointer to the memory region whose read lock is to be released.
/// @param s The size (in bytes) of the memory region.
void unlock_read(void *adr, size_t s);


/// @brief Requests a write lock for the specified memory region.
///
/// This function requests a write lock for the segment of memory of size `s` starting at address `adr`.
/// If one or more corresponding pages are locked for reading or writing by another node, this call blocks until the lock can be acquired.
///
/// @param adr A pointer to the memory region to lock for writing.
/// @param s The size (in bytes) of the memory region to lock.
void lock_write(void *adr, size_t s);


/// @brief Releases the write lock for the specified memory region.
///
/// This function releases a previously acquired write lock for the segment of memory of size `s` starting at address `adr`.
///
/// @param adr A pointer to the memory region whose write lock is to be released.
/// @param s The size (in bytes) of the memory region.
void unlock_write(void *adr, size_t s);
