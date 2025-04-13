#include "data_transfer.h"
#include "data_transfer_utils.h"

struct node_id *page_owners;

void set_new_owner(size_t page_id, struct node_id *new_owner) {
    pthread_mutex_lock(page_mtx + page_id);
    node_copy(page_owners + page_id, new_owner);
    pthread_mutex_unlock(page_mtx + page_id);
}

void sync_page(size_t page_id){
    // check if we are already the owner
    pthread_mutex_lock(page_mtx + page_id);
    struct node_id *owner = page_owners + page_id;
    if (node_equal(page_owners + page_id, &me)) {
        pthread_mutex_unlock(page_mtx + page_id);
        return;
    }
    page_state[page_id] = 1;
    pthread_mutex_unlock(page_mtx + page_id);
    
    // ask for a page and wait until the page is synched
    size_t ms_sz;
    struct message *msg = build_ASK_PAGE_message(page_id, &ms_sz);
    send_message(owner, msg, ms_sz);
    wait_signal(page_id);
    LOG_DATA_TRANS("synched page %zu\n", page_id);
    free_message(msg);
}

void init_data_transfer(unsigned int nb_pages, struct node_id* owners) {
    addHandler(RECV_PAGE, NULL, RECV_PAGE_handler);
    addHandler(ASK_PAGE, NULL, ASK_PAGE_handler);
    addHandler(ACK_RECV_PAGE, NULL, ACK_RECV_PAGE_handler);
    addHandler(RECV_PAGE_LEAVE, NULL, RECV_PAGE_LEAVE_handler);
    addHandler(DT_LEAVE, NULL, DT_LEAVE_handler);
    page_owners = malloc(nb_pages * sizeof(struct node_id));
    page_mtx = malloc(nb_pages * sizeof(pthread_mutex_t));
    page_cond = malloc(nb_pages * sizeof(pthread_cond_t));
    page_state = malloc(nb_pages * sizeof(char));
    for (unsigned int i = 0; i < nb_pages; i++) {
        pthread_mutex_init(page_mtx + i, NULL);
        pthread_cond_init(page_cond + i, NULL);
        page_state[i] = 0;
        if (!owners) node_copy(page_owners + i, &me);
    }
    if (owners) {
        memcpy(page_owners, owners, sizeof(struct node_id) * nb_pages);
    }
}

void clean_data_transfer() {
    free(page_owners);
    for (unsigned int i = 0; i < nb_pages; i++) {
        pthread_mutex_destroy(page_mtx + i);
        pthread_cond_destroy(page_cond + i);
    }
    free(page_mtx);
    free(page_cond);
    free(page_state);
}

void leave_data_transfer(struct node_id *new_owner) {
    // we wont treat any request further here
    addHandler(RECV_PAGE, NULL, NULL);
    addHandler(ASK_PAGE, NULL, NULL);
    LOG_DATA_TRANS("init will leave\n");
    for (size_t i = 0; i < nb_pages; i++) {
        // look for the pages we own
        if (node_equal(page_owners + i, &me)) {
            page_state[i] = 1;

            LOG_DATA_TRANS("Inform new Owner\n");
            size_t ms_sz;
            struct message *msg = build_PAGE_message(i, &ms_sz, new_owner, RECV_PAGE_LEAVE);
            send_message(new_owner, msg, ms_sz);
            free_message(msg);

            // wait for ack message
            wait_signal(i);
            LOG_DATA_TRANS("ACK recved\n");
            
            // broadcast to each other node, the info about the new owner
            struct node_list *n = &node_list;
            list_for_each_entry_continue(n, &node_list.nlist, nlist) {
                if (node_equal(&n->node, &me) || node_equal(&n->node, new_owner))
                    continue;
                // no mutex prot is needed because we are the only one using it here
                page_state[i] = 1;

                size_t ms_sz;
                struct message *msg = build_DT_LEAVE_message(i, new_owner, &ms_sz);
                send_message(&n->node, msg, ms_sz);
                
                // wait for ack2 message from users
                wait_signal(i);
                LOG_DATA_TRANS("ACK2 recved\n");
                free_message(msg);
            }
        }
    }

    clean_data_transfer();
}