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


const char *addr_init = "127.0.0.1";
const int init_port = 2451;
const int joiner_port = 4321;
const int nb_pages_ = 10;

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

int check_page_owners_equality() {
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
        
        Init_DSM(SIZE_DSM, init_port);

        log_info("INIT :  dsm ready");

        ASSERT_EQ(nb_pages, nb_pages_);


        ASSERT_EQ(list_empty(&node_list.nlist), 1);


        struct message * join_mess = wait_message(JOIN_DSM, NULL);
        log_info("message join recved from %d\n", join_mess->sender.port);

        eq = check_page_owners_equality();
        ASSERT_EQ(eq, 1);

        int *tab = (int *) dsm;
        *tab = 0;

        for (int *i = tab + 1; i < tab + 100; i++) {
            *i = *(i - 1) + (i - tab);
        }

        struct message * ask_page = wait_message(ASK_PAGE, NULL);

        log_info("mess ask recved\n");

        sleep(1);

        free_message(join_mess);
        free_message(ask_page);
        stop_server();
        clean_data_transfer();
        clean_core();
        free_nodes(&node_list);
        free_DSM();
    }else{
        // wait until INIT is setup
        sleep(1);

        join_DSM(addr_init, init_port, joiner_port);

        ASSERT_EQ(nb_pages, nb_pages_);

        int found = 0;

        eq = check_page_owners_equality();
        ASSERT_EQ(eq, 1);

        sleep(1);

        sync_page(page_owners, 0);

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
