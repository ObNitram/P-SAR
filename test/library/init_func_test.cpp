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
const int init_port = 1234;
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
        struct node_list *n1 = &node_list;
        int found =0;
        list_for_each_entry_continue(n1, &node_list.nlist, nlist) {
            ASSERT_EQ(n1->node.port != -1, 1);
            for (int i = 0; i<5; i++) {
                if (n1->node.port == join_mess->sender.port) {
                    found = 1;
                    break;
                }
            }
        }
        ASSERT_EQ(found, 1);

        waitpid(pid, NULL, 0);

    }else{
        sleep(3);
        join_DSM(addr_init, init_port, joiner_port);

        ASSERT_EQ(nb_pages, nb_pages_);

        int found = 0;
        
        ASSERT_EQ(nb_nodees, nb_nodes_);
        struct node_list *n1 = &node_list;

        list_for_each_entry_continue(n1, &node_list.nlist, nlist) {
            ASSERT_EQ(n1->node.port != -1, 1);
            for (int i = 0; i<5; i++) {
                if (n1->node.port == node_list_joiner[i].port) {
                    found = 1;
                    break;
                }
            }
            ASSERT_EQ(found, 1);
            found = 0;
        }

        for (int i = 0; i<nb_pages; i++) {
            lock_status s = (core_info + i)->mode;
            ASSERT_EQ(s, NONE);
        }




    }
}

