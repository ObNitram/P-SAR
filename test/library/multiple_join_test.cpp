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
static const int init_port = 2450;
static const int nb_nodes_ = 5;

struct node_id creator = {.host = "127.0.0.1", .port = init_port};
struct node_id joiner1 = {.host = "127.0.0.1", .port = init_port + 1};
struct node_id joiner2 = {.host = "127.0.0.1", .port = init_port + 2};
struct node_id joiner3 = {.host = "127.0.0.1", .port = init_port + 3};
struct node_id joiner4 = {.host = "127.0.0.1", .port = init_port + 4};
struct node_id joiner5 = {.host = "127.0.0.1", .port = init_port + 5};

struct node_id all_nodes[6] = {creator, joiner1, joiner2, joiner3, joiner4, joiner5};

static void wait_all(void) {
    for (unsigned int i = 0; i<nb_nodes_; i++)
        wait(NULL);
}

static void clear_DSM(void)
{
	stop_server();
	free_DSM();
	clean_data_transfer();
	clean_core();
	free_nodes(&node_list);
    nb_pages = 0;
    nb_nodees = 0;
} 

static int check_node_list_equality(int start_index) 
{
    struct node_list *n1 = &node_list;
    int found = 0;
    list_for_each_entry_continue(n1, &node_list.nlist, nlist) {
        for (int i = 0, index = start_index; i<nb_nodees; i++, index = (index + 1)%6) {
            if (n1->node.port, all_nodes[index].port) {
                found = 1;
                break;
            }
        }
        if (!found) return 0;
        found = 0;
    }
    return 1;
}

TEST(multiple_joins, try_join_the_same_node) {
    init_logger(stdout);

    log_info("started test\n");

    for (int i = 1; i<=5; i++) {
        pid_t pid = fork();
        if (!pid) {
            sleep(1);
            switch (i) {
                case 1:
                    join_DSM(creator.host, creator.port, LOCALHOST, joiner1.port);
                    break;

                case 2:
                    join_DSM(creator.host, creator.port, LOCALHOST, joiner2.port);
                    break;

                case 3:
                    join_DSM(creator.host, creator.port, LOCALHOST, joiner3.port);
                    break;

                case 4:
                    join_DSM(creator.host, creator.port, LOCALHOST, joiner4.port);
                    break;

                default:
                    join_DSM(creator.host, creator.port, LOCALHOST, joiner5.port);
                    break;
            }

            log_info("Node %d in the DSM\n", i);

            while(nb_nodees < nb_nodes_) {
                // a little dirty but we must wait here until
                // each node has joined the DSM
                sleep(1);
            }

            ASSERT_EQ(check_node_list_equality(i+1)%6, 1);

            clear_DSM();
            exit(0);
        }
    }

    Init_DSM(PAGE_SIZE, LOCALHOST, creator.port);

    log_info("me CREATOR ready\n");

    while(nb_nodees < nb_nodes_) {
        // a little dirty but we must wait here until
        // each node has joined the DSM
        sleep(1);
    }
    ASSERT_EQ(check_node_list_equality(0+1), 1);

    clear_DSM();
    wait_all();
}

TEST(multiple_joins, try_join_in_a_queue) {
    init_logger(stdout);

    log_info("started test\n");

    for (int i = 1; i<=5; i++) {
        pid_t pid = fork();
        if (!pid) {
            sleep(i);
            switch (i) {
                case 1:
                    join_DSM(creator.host, creator.port, LOCALHOST, joiner1.port);
                    break;

                case 2:
                    join_DSM(joiner1.host, joiner1.port, LOCALHOST, joiner2.port);
                    break;

                case 3:
                    join_DSM(joiner2.host, joiner2.port, LOCALHOST, joiner3.port);
                    break;

                case 4:
                    join_DSM(joiner3.host, joiner3.port, LOCALHOST, joiner4.port);
                    break;

                default:
                    join_DSM(joiner4.host, joiner4.port, LOCALHOST, joiner5.port);
                    break;
            }

            log_info("Node %d in the DSM\n", i);

            while(nb_nodees < nb_nodes_) {
                // a little dirty but we must wait here until
                // each node has joined the DSM
                sleep(1);
            }

            ASSERT_EQ(check_node_list_equality(i+1)%6, 1);

            clear_DSM();
            exit(0);
        }
    }

    Init_DSM(PAGE_SIZE, LOCALHOST, creator.port);

    log_info("me CREATOR ready\n");

    while(nb_nodees < nb_nodes_) {
        // a little dirty but we must wait here until
        // each node has joined the DSM
        sleep(1);
    }
    ASSERT_EQ(check_node_list_equality(0+1), 1);

    clear_DSM();
    wait_all();
}

TEST(multiple_joins, try_join_any_node) {
    init_logger(stdout);

    log_info("started test\n");

    for (int i = 1; i<=5; i++) {
        pid_t pid = fork();
        if (!pid) {
            sleep(i);
            switch (i) {
                case 1:
                    join_DSM(creator.host, creator.port, LOCALHOST, joiner1.port);
                    break;

                case 2:
                    join_DSM(joiner1.host, joiner1.port, LOCALHOST, joiner2.port);
                    break;

                case 3:
                    join_DSM(joiner1.host, joiner1.port, LOCALHOST, joiner3.port);
                    break;

                case 4:
                    join_DSM(joiner2.host, joiner2.port, LOCALHOST, joiner4.port);
                    break;

                default:
                    join_DSM(joiner3.host, joiner3.port, LOCALHOST, joiner5.port);
                    break;
            }

            log_info("Node %d in the DSM\n", i);

            while(nb_nodees < nb_nodes_) {
                // a little dirty but we must wait here until
                // each node has joined the DSM
                sleep(1);
            }

            ASSERT_EQ(check_node_list_equality(i+1)%6, 1);

            clear_DSM();
            exit(0);
        }
    }

    Init_DSM(PAGE_SIZE, LOCALHOST, creator.port);

    log_info("me CREATOR ready\n");

    while(nb_nodees < nb_nodes_) {
        // a little dirty but we must wait here until
        // each node has joined the DSM
        sleep(1);
    }
    ASSERT_EQ(check_node_list_equality(0+1), 1);

    clear_DSM();
    wait_all();
}