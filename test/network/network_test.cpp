#include <gtest/gtest.h>
#include <pthread.h>
#include <unistd.h>

extern "C" {
#include "network/network.h"
#include "network/message.h"
#include "utils/logger.h"
#include "network/cond_var.h"
}

const char *localhost = "127.0.0.1";
const char *str = "Hello, this is a test message !";
struct node_id dest {
	"127.0.0.1", 5555
};

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
	start_server(5555, localhost);

	addHandler(1, NULL, callBack);

	log_info("handler setup");

	sleep(1);

	struct message mes = {};
	mes.message_type = 1;

	send_message(&dest, &mes, sizeof(mes));

	log_info("message send");

	sleep(1);

	stop_server();

	EXPECT_EQ(counter, 1);
}

TEST(network, must_not_receive_if_message_number_is_different)
{
	init_logger(stderr);
	counter = 0;
	start_server(5555, localhost);

	addHandler(1, NULL, callBack);

	struct message mes = {};
	mes.message_type = 2;

	send_message(&dest, &mes, sizeof(mes));

	log_info("stop server");

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
	start_server(5555, localhost);

	addHandler(2, NULL, callBack2);

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

struct cond_var send_wait_for_message_cond;
void handler_send_wait_for_message(struct message *data)
{
	counter++;

	pthread_mutex_lock(&send_wait_for_message_cond.lock);

	struct message2 *cast_message = (struct message2 *)data;

	EXPECT_EQ(cast_message->message.message_type, 2);
	EXPECT_TRUE(cast_message->message.sender.host != NULL);
	EXPECT_TRUE(strcmp(cast_message->message.sender.host, localhost) == 0);

	EXPECT_EQ(cast_message->data, 42);
	EXPECT_TRUE(cast_message->data == 42);

	EXPECT_EQ(cast_message->data2, 24);
	EXPECT_TRUE(cast_message->data2 == 24);

	EXPECT_TRUE(strcmp(cast_message->data3, localhost) == 0);
	sleep(1);

	send_wait_for_message_cond.predicate = true;
	pthread_cond_signal(&send_wait_for_message_cond.cond);
	pthread_mutex_unlock(&send_wait_for_message_cond.lock);
}

TEST(network, send_wait_message)
{
	init_logger(stdout);
	counter = 0;
	start_server(5555, localhost);

	addHandler(2, NULL, handler_send_wait_for_message);

	struct message2 message = {};
	message.message.message_type = 2;
	message.data = 42;
	message.data2 = 24;
	strcpy(message.data3, localhost);

	send_wait_message(&dest, (struct message *)&message, sizeof(message), &send_wait_for_message_cond);

	stop_server();

	EXPECT_EQ(counter, 1);
}


struct cond_var handlerception_cond = COND_VAR_INIT;
void handlerception(struct message *data){
	pthread_mutex_lock(&handlerception_cond.lock);

	counter++;
	sleep(1);

	handlerception_cond.predicate = true;
	pthread_cond_signal(&handlerception_cond.cond);
	pthread_mutex_unlock(&handlerception_cond.lock);
}

struct cond_var waiting_handler_cond = COND_VAR_INIT;
void waiting_handler(struct message *data){
	pthread_mutex_lock(&waiting_handler_cond.lock);

	counter++;
	struct message2 message = {};
	message.message.message_type = 1;
	message.data = 42;
	message.data2 = 24;
	strcpy(message.data3, localhost);

	send_wait_message(&dest, (struct message *)&message, sizeof(message), &handlerception_cond);

	waiting_handler_cond.predicate = true;
	pthread_cond_signal(&waiting_handler_cond.cond);
	pthread_mutex_unlock(&waiting_handler_cond.lock);
}

TEST(network, send_wait_message_in_handler)
{
	init_logger(stdout);
	counter = 0;
	start_server(5555, localhost);

	addHandler(2, NULL, waiting_handler);
	addHandler(1, NULL, handlerception);

	struct message2 message = {};
	message.message.message_type = 2;
	message.data = 42;
	message.data2 = 24;
	strcpy(message.data3, localhost);

	send_wait_message(&dest, (struct message *)&message, sizeof(message), &waiting_handler_cond);

	stop_server();

	EXPECT_EQ(counter, 2);
}