#pragma once

#include "../network/message.h"

/// @brief enum for the mode requested => export to library ?
/// @details
enum lock_type { READ, WRITE };

/// @brief enum for local status of lock
/// @details
enum lock_status {
	READING = READ,
	WRITING = WRITE,
	NONE,
};

/// @brief
/// @details
/// @param page_id
/// @param lock_type
/// @return 0 on success, -1 if we dont have the lock
void ask_lock(const size_t page_id, const enum lock_type lock_type);

/// @brief
/// @details
/// @param page_id
/// @param lock_type
void unlock(const size_t page_id, const enum lock_type lock_type);

/// @brief Init the reader/writer module with the number of page
/// @details
/// @param nb_pages The number of pages to manage on the same critical section.
/// @param have_token The node id who have the critical section on the begining.
void init_core(const size_t nb_pages, const struct node_id *have_token);

/// @brief Clean the inner state of the reader/writer module
/// @details
void clean_core(void);

/// @brief
/// @details
/// @param page_id
enum lock_status get_lock_status(const size_t page_id);

/// @brief leave the peer to peer network
/// @details
/// @param delegate
int exit_core(const struct node_id delegate);