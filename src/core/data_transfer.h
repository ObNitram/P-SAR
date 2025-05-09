#pragma once

#include "../network/utils/node_id.h"

/// @brief An array that contains for each page it's owner
extern struct node_id *page_owners;

/// @brief Sets a new owner for a given page
/// @param page_id The id of the page to be set
/// @param new_owner The new owner of the page
extern void set_new_owner(size_t page_id, struct node_id *new_owner);

/// @brief Synchronizes a page with its owner
/// @param page_id The id of the page to be synchronized
extern void sync_page(size_t page_id);

/// @brief Initializes the data transfer module
/// @param nb_pages The number of pages to be managed
/// @param owners An array of node_id structures representing the owners of each page
/// @details If owners is NULL, the current node is set as the owner of all pages.
extern void init_data_transfer(unsigned int nb_pages, struct node_id *owners);

/// @brief Cleans up the data transfer module
/// @details This function frees the allocated memory for the data transfer module.
extern void clean_data_transfer();

/// @brief Leaves the data transfer module and cleans it
/// @details We assume that this function is only called once.
extern void exit_data_transfer(void);

extern void send_invalidation(size_t page_index);

/// @brief Gets the owners of the pages
/// @param dst A pointer to the destination where the page owners will be stored
/// @details This function copies the current page owners into the provided destination.
///          It is assumed that the destination has enough space to hold the page owners.
extern void get_page_owners(void *dst);
