#pragma once

#include <stddef.h>
#include "message.h"

/// @brief Starts the server on the specified port.
/// @details Initializes the server and begins listening for incoming connections on the given port.
///          This function should be called before attempting to send or receive messages.
/// @param port The port number on which the server will listen.
void start_server(int port);

/// @brief Stops the running server.
/// @details Gracefully stops the server by closing all connections and terminating the server thread.
///          After calling this function, the server will no longer accept new connections.
void stop_server();

/// @brief Retrieves the port number on which the server is running.
/// @details Returns the current server port number that was set during the call to start_server.
/// @return The port number on which the server is listening, or -1 if the server is not running.
int get_server_port();

/// @brief Retrieves the IP address of the server.
/// @details Returns a string containing the IP address of the server. The returned string should be
///          freed by the caller when it is no longer needed.
/// @return A pointer to a dynamically allocated string containing the server IP address, or NULL on error.
char *get_server_ip();

/// @brief Sends a message to a specified destination node.
/// @details Sends the provided message to the destination node identified by the node_id structure.
///          This function sends first the size of the message followed by the actual message data.
/// @param dest Pointer to the destination node identifier.
/// @param message Pointer to the message to be sent.
/// @param message_size Size of the message in bytes.
void send_message(const struct node_id *dest,
                  struct message *message,
                  size_t message_size);

/// @brief Waits for a message of a specific type.
/// @details This function blocks until a message of the specified type is received.
///          If the sender parameter is NULL, it waits for a message from any sender.
/// @param message_type The type of message to wait for.
/// @param sender Pointer to the node identifier of the expected sender, or NULL to accept any sender.
/// @return A pointer to a dynamically allocated struct message containing the received message details.
///         The caller is responsible for freeing the memory.
struct message *wait_message(size_t message_type, struct node_id *sender);

/// @brief Adds a handler for messages of a specific type.
/// @details Registers a callback function that will be invoked when a message of the specified type is received.
///          If the sender parameter is NULL, the handler will be triggered for messages from any sender.
/// @param message_type The type of message for which the handler is registered.
/// @param sender Pointer to the node identifier of the sender to filter on, or NULL for any sender.
/// @param callBack The callback function to be invoked when the message is received.
void addHandler(size_t message_type,
                struct node_id *sender,
                void callBack(struct message *message));
