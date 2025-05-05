#include <cstdio>
#include <gtest/gtest.h>

extern "C" {
#include "comm/comm.h"
#include "utils/logger.h"
#include "notification/notification.h"
#include "network/utils/cond_var.h"
}

static void dummy(int fd)
{
}

TEST(NotificationTest, CreateDestroyChan)
{
	init_logger(stdout);
	init_comm();
	int fd = create_chan(dummy);
	ASSERT_GE(fd, -1);
	EXPECT_EQ(destroy_chan(dummy, fd), 0);
	exit_comm();
}

struct cond_var wait_until_msg_receive = COND_VAR_INIT;
static void sendInt(int fd)
{
    log_debug("message receive");
	int vals[5] = { 0 };
	EXPECT_EQ(read(fd, vals, sizeof(vals)), sizeof(vals));
	unlock_cond(&wait_until_msg_receive, false);
	EXPECT_EQ(vals[0], 1);
    EXPECT_EQ(vals[1], 2);
    EXPECT_EQ(vals[2], 3);
    EXPECT_EQ(vals[3], 4);
    EXPECT_EQ(vals[4], 5);
}
TEST(NotificationTest, WriteReadInChan)
{
	init_logger(stdout);
	init_comm();

	int fd = create_chan(sendInt);
	ASSERT_GE(fd, -1);

    int vals[5] = { 1, 2, 3, 4, 5};

	EXPECT_EQ(write(fd, vals, sizeof(vals)), sizeof(vals));

	wait_on_cond(&wait_until_msg_receive, false);

	EXPECT_EQ(destroy_chan(sendInt, fd), 0);
	exit_comm();
}