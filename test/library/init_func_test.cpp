#include <gtest/gtest.h>

extern "C" {
#include "library.h"
#include "network/network.h"
#include "network/message.h"
#include "utils/list.h"
#include "utils/logger.h"
#include "core/core.h"
}


const char *addr_init = "127.0.0.1";
const int init_port = 2451;
const int joiner_port = 4321;
const int nb_pages_ = 10;
const int nb_nodes_ = 5;

// the final node_list of JOINER
struct node_id node_list_joiner[5] = {
    {.host = "127.0.0.1", .port =init_port}, 
    {.host = "127.0.0.1", .port =init_port+17},
    {.host = "127.0.0.1", .port =init_port+7},
    {.host = "127.0.0.1", .port =init_port+5}, 
    {.host = "127.0.0.1", .port =init_port+4}, 
};

// the final node_list of init
struct node_id node_list_init[5] = {
    {.host = "127.0.0.1", .port =joiner_port}, 
    {.host = "127.0.0.1", .port =init_port+4}, 
    {.host = "127.0.0.1", .port =init_port+5}, 
    {.host = "127.0.0.1", .port =init_port+7},
    {.host = "127.0.0.1", .port =init_port+17},
};

#define SIZE_DSM 40960

int check_node_list_equality(struct node_id nodes[]) {
    struct node_list *n1 = &node_list;
    int found = 0;
    list_for_each_entry_continue(n1, &node_list.nlist, nlist) {
        assert(n1->node.port != -1);
        for (int i = 0; i<5; i++) {
            if (n1->node.port == nodes[i].port) {
                found = 1;
                break;
            }
        }
        if (!found) return 0;
        found = 0;
    }
    return 1;
}

// we have two proc
// one that inits the DSM, the other who will try to join it
TEST(join_init_dsm, try_to_init_then_join_the_dsm)
{
    init_logger(stdout);

    log_info("started test\n");

    pid_t pid = fork();
    if (pid) {
        
        Init_DSM(SIZE_DSM, init_port);

        log_info("INIT :  dsm ready");

        ASSERT_EQ(nb_pages, nb_pages_);


        ASSERT_EQ(list_empty(&node_list.nlist), 1);

        //we add some nodes
        for (int i = 4; i>0; i--) {
            add_to_nodes(node_list_init[i].host, node_list_init[i].port);
        }

        struct message * join_mess = wait_message(JOIN_DSM, NULL);
        log_info("message join recved from %d\n", join_mess->sender.port);

        // check that node_list is the same as 
        int eq = check_node_list_equality(node_list_init);
        ASSERT_EQ(eq, 1);

    }else{
        // wait until INIT is setup
        sleep(1);

        join_DSM(addr_init, init_port, joiner_port);

        ASSERT_EQ(nb_pages, nb_pages_);

        int found = 0;
        
        ASSERT_EQ(nb_nodees, nb_nodes_);
        int eq = 0;


        eq = check_node_list_equality(node_list_joiner);
        ASSERT_EQ(eq, 1);

        eq = check_core_info_test();
        ASSERT_EQ(eq, 1);
    }
}

