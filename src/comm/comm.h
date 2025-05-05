#pragma once

#include "utils/list.h"

/// @brief init the main loop event
/// @return 0 on success, -1 on failure
int init_comm(void);

/// @brief exit the main loop event
/// @return 0 on success, -1 on failure
int exit_comm(void);

/// @brief add a new event to watch
/// @return 0 on success, -1 on failure
int add_handler(void (*callback)(int fd), int fd);

/// @brief remove event from the list of watch events
/// @return 0 on success -1 on failure
int delete_handler(int fd);

/// @brief remove events from the list of watch events
/// @return 0 on success -1 on failure
int delete_handlers(void (*callback)(int fd));