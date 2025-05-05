#include <gtest/gtest.h>
#include <sys/eventfd.h>
#include <utils/list.h>
#include <thread>
#include <atomic>
#include <vector>
#include <unistd.h>

extern "C" {
#include "comm/comm.h"
#include "utils/logger.h"
}

// Global storage for callback dispatch
static const int MAX_TEST = 64;
static int g_fds[MAX_TEST];
static std::atomic<bool> g_flags[MAX_TEST];
static int g_n = 0;

// Dispatcher: sets the flag corresponding to fd
static void fd_dispatch_cb(int fd) {
    for (int i = 0; i < g_n; ++i) {
        if (g_fds[i] == fd) {
            g_flags[i].store(true);
            return;
        }
    }
}

// No-op callback
static void noop_cb(int fd) {
    (void)fd;
}

TEST(CommTest, InitAndExit) {
    init_logger(stdout);
    EXPECT_EQ(init_comm(), 0);
    EXPECT_EQ(exit_comm(), 0);
}

TEST(CommTest, ExitWithoutInit) {
    init_logger(stdout);
    EXPECT_EQ(exit_comm(), -1);
}

TEST(CommTest, DoubleInit) {
    init_logger(stdout);
    EXPECT_EQ(init_comm(), 0);
    EXPECT_EQ(init_comm(), -1);
    EXPECT_EQ(exit_comm(), 0);
}

TEST(CommTest, AddBeforeInit) {
    init_logger(stdout);
    int fd = eventfd(0, 0);
    ASSERT_GE(fd, 0);
    EXPECT_EQ(add_handler(noop_cb, fd), -1);
    close(fd);
}

TEST(CommTest, AddAndDeleteHandler) {
    ASSERT_EQ(init_comm(), 0);

    int fd = eventfd(0, 0);
    ASSERT_GE(fd, 0);

    // add and delete a no-op handler
    EXPECT_EQ(add_handler(noop_cb, fd), 0);
    EXPECT_EQ(delete_handler(fd), 0);

    close(fd);
    EXPECT_EQ(exit_comm(), 0);
}

TEST(CommTest, DeleteUnknownHandler) {
    init_logger(stdout);
    ASSERT_EQ(init_comm(), 0);
    int fd = eventfd(0, 0);
    ASSERT_GE(fd, 0);
    EXPECT_EQ(delete_handler(fd), -1);
    close(fd);
    EXPECT_EQ(exit_comm(), 0);
}

TEST(CommTest, EventLoopInvokeCallback) {
    ASSERT_EQ(init_comm(), 0);

    int efd = eventfd(0, 0);
    ASSERT_GE(efd, 0);

    // Prepare global dispatch arrays
    g_n = 1;
    g_fds[0] = efd;
    g_flags[0].store(false);

    EXPECT_EQ(add_handler(fd_dispatch_cb, efd), 0);

    // trigger event
    eventfd_write(efd, 1);

    // wait for callback invocation
    for (int i = 0; i < 50 && !g_flags[0].load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    EXPECT_TRUE(g_flags[0].load());

    EXPECT_EQ(delete_handler(efd), 0);
    close(efd);
    EXPECT_EQ(exit_comm(), 0);
}

TEST(CommTest, ConcurrentAddHandlers) {
    ASSERT_EQ(init_comm(), 0);

    const int num = 5;
    std::vector<int> fds(num);
    g_n = num;
    for (int i = 0; i < num; ++i) {
        fds[i] = eventfd(0, 0);
        ASSERT_GE(fds[i], 0);
        g_fds[i] = fds[i];
        g_flags[i].store(false);
    }

    // concurrently add handlers
    std::vector<std::thread> threads;
    for (int i = 0; i < num; ++i) {
        threads.emplace_back([i, &fds](){
            EXPECT_EQ(add_handler(fd_dispatch_cb, fds[i]), 0);
        });
    }
    for (auto &t : threads) t.join();
    threads.clear();

    // trigger events
    for (int fd : fds) eventfd_write(fd, 1);

    // wait for all callbacks
    for (int i = 0; i < num; ++i) {
        for (int j = 0; j < 50 && !g_flags[i].load(); ++j) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        EXPECT_TRUE(g_flags[i].load());
    }

    // concurrently delete handlers and close fds
    for (int i = 0; i < num; ++i) {
        threads.emplace_back([i, &fds](){
            EXPECT_EQ(delete_handler(fds[i]), 0);
            close(fds[i]);
        });
    }
    for (auto &t : threads) t.join();

    EXPECT_EQ(exit_comm(), 0);
}