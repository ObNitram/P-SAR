#include <gtest/gtest.h>

extern "C" {
#include "network/network.h"
#include "utils/logger.h"
}


static int counter = 0;

void callBack(struct message *message)
{
	counter++;
	EXPECT_EQ(message->message_type, 1);

	EXPECT_EQ(message->message_size, 0);

	free_message(message);
}

struct node_id dest{ "127.0.0.1", 5555 };
char *message = "Hello, this is a test message !";


TEST(network, basic_receive)
{
	init_logger(stderr);
	counter = 0;
	start_server();

	addHandler(1,NULL, callBack);

	sleep(1);

	send_message(1, &dest, NULL, 0);

	stop_server();

	EXPECT_EQ(counter, 1);
}

TEST(network, must_not_receive_if_message_number_is_different)
{
	init_logger(stderr);
	counter = 0;
	start_server();

	addHandler(1,NULL, callBack);

	sleep(1);

	send_message(2, &dest, NULL, 0);

	stop_server();

	EXPECT_EQ(counter, 0);
}

void callBack2(struct message *mes)
{
	counter++;
	EXPECT_EQ(mes->message_type, 2);

	EXPECT_EQ(mes->message_size, strlen(message) +1);

	EXPECT_EQ(mes->message_size, strlen((char*)&mes->message_data) + 1);

	EXPECT_TRUE(strcmp((char*)&mes->message_data,message) == 0);

	free_message(mes);
}

TEST(network, basic_receive2)
{
	init_logger(stderr);
	counter = 0;
	start_server();

	addHandler(2,NULL, callBack2);

	sleep(1);

	send_message(2, &dest, message, strlen(message) + 1);

	stop_server();

	EXPECT_EQ(counter, 1);
}

TEST(network, wait_for_message)
{
	init_logger(stderr);
	start_server();

	sleep(1);

	send_message(2, &dest, message, strlen(message) + 1);

	struct message *rep = wait_message(2,NULL);

	EXPECT_EQ(rep->message_type, 2);
	EXPECT_EQ(rep->message_size, strlen(message) + 1);
	EXPECT_EQ(rep->message_size, strlen((char*)&rep->message_data) + 1);
	EXPECT_TRUE(strcmp((char*)&rep->message_data,message) == 0);

	free_message(rep);

	stop_server();

	EXPECT_EQ(counter, 1);
}