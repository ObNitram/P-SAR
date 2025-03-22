#include <gtest/gtest.h>

extern "C" {
#include "network/network.h"
#include "network/message.h"
#include "utils/logger.h"
}


const char *localhost = "127.0.0.1";
const char *str = "Hello, this is a test message !";
struct node_id dest{ "127.0.0.1", 5555 };


static int counter = 0;

void callBack(struct message *message)
{
	counter++;
	EXPECT_EQ(message->message_type, 1);

	EXPECT_TRUE(message->sender.host != NULL);

	EXPECT_TRUE(strcmp(message->sender.host, localhost) == 0);

}


TEST(network, basic_receive)
{
	init_logger(stderr);
	counter = 0;
	start_server(5555);

	addHandler(1,NULL, callBack);

	sleep(1);

	struct message mes = {};
	mes.message_type = 1;

	send_message(&dest, &mes, sizeof(mes));

	sleep(1);

	stop_server();

	EXPECT_EQ(counter, 1);
}

TEST(network, must_not_receive_if_message_number_is_different)
{
	init_logger(stderr);
	counter = 0;
	start_server(5555);

	addHandler(1,NULL, callBack);

	sleep(1);

	struct message mes = {};
	mes.message_type = 2;

	send_message(&dest, &mes, sizeof(mes));

	stop_server();

	EXPECT_EQ(counter, 0);
}


struct message2 {
	struct message message;
	int data;
	int data2;
	char data3[1024];
};


void callBack2(struct message *mes)
{
	counter++;
	struct message2 *cast_message = (struct message2 *)mes;

	EXPECT_EQ(cast_message->message.message_type, 2);
	EXPECT_TRUE(cast_message->message.sender.host != NULL);
	EXPECT_TRUE(strcmp(cast_message->message.sender.host, localhost) == 0);

	EXPECT_EQ(cast_message->data, 42);
	EXPECT_TRUE(cast_message->data == 42);

	EXPECT_EQ(cast_message->data2, 24);
	EXPECT_TRUE(cast_message->data2 == 24);

	EXPECT_TRUE(strcmp(cast_message->data3, localhost) == 0);

}

TEST(network, basic_receive2)
{
	init_logger(stderr);
	counter = 0;
	start_server(5555);

	addHandler(2,NULL, callBack2);

	sleep(1);

	struct message2 mes = {};
	mes.message.message_type = 2;
	mes.data = 42;
	mes.data2 = 24;
	strcpy(mes.data3, localhost);

	send_message(&dest, (struct message *)&mes, sizeof(mes));

	sleep(1);

	stop_server();

	EXPECT_EQ(counter, 1);
}


void *lunch_message(void *)
{
	sleep(1);
	struct message2 message = {};
	message.message.message_type = 2;
	message.data = 42;
	message.data2 = 24;
	strcpy(message.data3, localhost);

	send_message(&dest, (struct message *)&message, sizeof(message));

	return NULL;
}


TEST(network, wait_for_message)
{
	init_logger(stderr);
	counter = 1;
	start_server(5555);

	sleep(1);

	static pthread_t server_thread_id;
	pthread_create(&server_thread_id, NULL, lunch_message, NULL);

	struct message2 *cast_message = (struct message2 *)wait_message(2,NULL);

	EXPECT_EQ(cast_message->message.message_type, 2);
	EXPECT_TRUE(cast_message->message.sender.host != NULL);
	EXPECT_TRUE(strcmp(cast_message->message.sender.host, localhost) == 0);

	EXPECT_EQ(cast_message->data, 42);
	EXPECT_TRUE(cast_message->data == 42);

	EXPECT_EQ(cast_message->data2, 24);
	EXPECT_TRUE(cast_message->data2 == 24);

	EXPECT_TRUE(strcmp(cast_message->data3, localhost) == 0);

	free_message((struct message *)cast_message);

	stop_server();

	EXPECT_EQ(counter, 1);

	pthread_join(server_thread_id, NULL);
}