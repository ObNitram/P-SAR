#pragma once

#include <stddef.h>
#define LOCALHOST "127.0.0.1"

/// @brief Initializes the distributed shared memory for the initial node with the specified size.
///
/// This function allocates and initializes a distributed shared memory region for the initial node.
///
/// @param size The size (in bytes) of the shared memory to initialize.
/// @param interface The interface for binding socket, it ensure that the node have only one id.
/// @param port The port you use to communicate
/// @return A pointer to the allocated shared memory region on success, NULL otherwise.
void *Init_DSM(size_t size, const char *interface, int port);

/// @brief Adds a new node to the distributed shared memory system by connecting to an existing node.
///
/// This function connects the new node to the distributed shared memory system by contacting
/// the node at the specified host address and returns the address of the shared memory region.
///
/// @param host The hostname or IP address of the existing node to connect to.
/// @param connect_port The port of the existing node to connect to.
/// @param interface The interface for binding socket, it ensure that the node have only one id.
/// @param server_port The port you listen to add a new node
/// @return A pointer to the shared memory region.
void *join_DSM(const char *host, int connect_port, const char *interface,
	       int server_port);

/// @brief Leaves the distributed shared memory system, by transmetting it's actual state to a successor.
///
/// This function frees each mallocated memory and stop the server.
/// @return NULL if we are not the last node in the DSM, otherwise returns the adresse of the final state of the DSM.
/// @note The return value must be freed.
void *leave_DSM(void);

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
