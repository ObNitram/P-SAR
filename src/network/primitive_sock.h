#pragma once

#include <sys/socket.h>

/// @brief Receive N byte of data on the given socket, protect to signal.
/// @param sock The socket to read.
/// @param data the data to write data, it must be initialize before with the given size.
/// @param size The size to read in the socket.
/// @return the size read or -1 on failure, a size of 0 means EOF.
static int Recv_all(const int sock, void *data, const size_t size)
{
	int seek = 0;
	int ret = 0;
	do {
		ret = recv(sock, data + seek, size - seek, 0);
		if (ret == 0 && seek == 0)
			break;
		if (ret == -1)
			return -1;
		seek += ret;
	} while ((size - seek) > 0);
	return seek;
}

/// @brief Receive N byte of data on the given socket, protect to signal.
/// @param sock The socket to read.
/// @param size The size to read in the socket.
/// @return the size read or -1 on failure, a size of 0 means EOF.
static int Clean_all(const int sock, const size_t size)
{
	int seek = 0;
	int ret = 0;
	char buff[32];
	do {
		ret = recv(sock, buff, sizeof(buff), 0);
		if (ret == 0 && seek == 0)
			break;
		if (ret == -1)
			return -1;
		seek += ret;
	} while ((size - seek) > 0);
	return seek;
}

/// @brief Send N byte of data on the given socket, protect to signal.
/// @param sock The socket to send.
/// @param data the data to write data, it must be initialize before with the given size.
/// @param size The size of data to send in the socket.
/// @return 0 on success or -1 on failure.
static int send_all(const int sockfd, const char *data, const size_t size)
{
	int seek = 0;
	int ret = 0;
	do {
		ret = send(sockfd, data + seek, size - seek, MSG_NOSIGNAL);
		if (ret == -1)
			return -1;
		seek += ret;
	} while ((size - seek) > 0);
	return 0;
}