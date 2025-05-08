#include <gtest/gtest.h>

extern "C" {
#include "network/utils/network_header.h"
}

TEST(networkSerialize, SerializeUnserialize)
{
	struct header src = {
        .message_type= 1,
        .sender = {
            .host = "127.0.0.1",
            .port = 8080,
        }
    }, dest = { 0 };
	char buffer[HEADER_SIZE] = { 0 };

	serialize_header(buffer, &src);
	unserialize_header(&dest, buffer);

	EXPECT_TRUE(node_equal(&src.sender, &dest.sender));
	EXPECT_TRUE(src.message_type == dest.message_type);
}
