#include <gtest/gtest.h>

extern "C" {
#include "library.h"
#include "network/network.h"
#include "network/message.h"
#include "utils/list.h"
#include "utils/logger.h"
#include "core/core.h"
#include "core/data_transfer.h"
}


static const char *addr_init = "127.0.0.1";
static const int init_port = 2451;
static const int joiner_port = 4321;
static const int nb_pages_ = 10;

struct node_id page_owners_both[10] = {
    {.host = "127.0.0.1", .port =init_port}, 
    {.host = "127.0.0.1", .port =init_port}, 
    {.host = "127.0.0.1", .port =init_port}, 
    {.host = "127.0.0.1", .port =init_port}, 
    {.host = "127.0.0.1", .port =init_port}, 
    {.host = "127.0.0.1", .port =init_port}, 
    {.host = "127.0.0.1", .port =init_port}, 
    {.host = "127.0.0.1", .port =init_port}, 
    {.host = "127.0.0.1", .port =init_port}, 
    {.host = "127.0.0.1", .port =init_port}
};

#define SIZE_DSM 40960

static int check_page_owners_equality() {
    for(int i = 0; i<nb_pages_; i++) {
        if (page_owners[i].port != page_owners_both[i].port) {
            return 0;
        }
    }
    return 1;
}

// we have two proc
// one that inits the DSM, the other who will try to join it
TEST(data_transfer, join_then_try_sync_a_page)
{
    init_logger(stdout);

    log_info("started test\n");

    pid_t pid = fork();
    int eq = 0;
    if (pid) {
        
        Init_DSM(SIZE_DSM, LOCALHOST, init_port);

        eq = check_page_owners_equality();
        ASSERT_EQ(eq, 1);

        // edit some data
        int *tab = (int *) dsm;
        *tab = 0;
        for (int *i = tab + 1; i < tab + 100; i++) {
            *i = *(i - 1) + (i - tab);
        }

        // wait te recv an ASK_PAGE request
        sleep(3);

        stop_server();
        clean_data_transfer();
        clean_core();
        free_nodes(&node_list);
        free_DSM();
        wait(NULL);
    }else{
        // wait until INIT is setup
        sleep(1);

        join_DSM(addr_init, init_port, LOCALHOST, joiner_port);

        log_info("joined the DSM\n");

        ASSERT_EQ(nb_pages, nb_pages_);

        int found = 0;

        eq = check_page_owners_equality();
        ASSERT_EQ(eq, 1);

        sleep(1);

        sync_page(0);

        log_info("synced page 0\n");

        int *tab = (int *) dsm;
        ASSERT_EQ(*tab, 0);

        eq = 0;

        for (int *i = tab + 1; i < tab + 100; i++) {
            int calculated_val = *(i - 1) + (i - tab);
            int cur_val = *i;
            eq = calculated_val == cur_val;
            if (!eq) break;
        }

        ASSERT_EQ(eq, 1);

        stop_server();
        clean_data_transfer();
        clean_core();
        free_nodes(&node_list);
        free_DSM();

        exit(0);
    }
}
