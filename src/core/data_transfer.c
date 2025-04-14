#include "data_transfer.h"
#include "../utils/utils.h"
#include "../sigsegv_handler/sigsegv.h"
#include <stdlib.h>

struct node_id *page_owners;

static void RECV_PAGE_handler(struct message *message) {
    size_t *page_id = (size_t *) (message + 1);
    void *addr_np = (void *) (page_id + 1);
    void *addr_p = dsm + (*page_id) * PAGE_SIZE;
    node_copy(page_owners + *page_id, &message->sender);
    memory_unlock_write(*page_id);
    memcpy(addr_p, addr_np, PAGE_SIZE);
    memory_lock_reset(*page_id);
}

static void transfer_page(struct node_id *requester, size_t page_id) {
    void *addr_pg = dsm + PAGE_SIZE * page_id;
    size_t ms_sz = sizeof(struct message) + sizeof(size_t)
                                          + PAGE_SIZE;
    struct message *msg = malloc(ms_sz);
    msg->message_type = RECV_PAGE;
    size_t *index_p = (size_t * ) (msg + 1);
    *index_p = page_id;
    memory_unlock_read(page_id);
    memcpy(index_p + 1, addr_pg, PAGE_SIZE);
    memory_lock_reset(page_id);
    send_message(requester, msg, ms_sz);
    free_message(msg);
}

static void ASK_PAGE_handler(struct message *message) {
    size_t *page_id = (size_t *) (message + 1);
    struct node_id *requester = (struct node_id *) (page_id + 1);
    struct node_id *owner = page_owners + *page_id;
    if (node_equal(owner, &me)) {
        transfer_page(requester, *page_id);
    }else{
        send_message(owner, message, 
            sizeof(struct message) + sizeof(size_t) + 
            sizeof(struct node_id));
    }
}

void sync_page(size_t page_id){
    // TODO set a dirty bit
    if (node_equal(page_owners + page_id, &me)) {
        return;
    }
    size_t ms_sz =  sizeof(struct message) + sizeof(size_t) +
                    sizeof(struct node_id);
    struct message *msg = malloc(ms_sz);
    msg->message_type = ASK_PAGE;
    size_t * index_p = (size_t *) (msg + 1);
    *index_p = page_id;
    node_copy((struct node_id *) (index_p + 1), &me);
    send_message(page_owners + page_id, msg, ms_sz);
    free_message(wait_message(RECV_PAGE, NULL));
    LOG_DATA_TRANS("synced page %zu\n", page_id);
    free_message(msg);
}

void init_data_transfer(unsigned int nb_pages, struct node_id* owners) {
    addHandler(RECV_PAGE, NULL, RECV_PAGE_handler);
    addHandler(ASK_PAGE, NULL, ASK_PAGE_handler);
    page_owners = malloc(nb_pages * sizeof(struct node_id));
    if (owners) {
        memcpy(page_owners, owners, sizeof(struct node_id) * nb_pages);
    }else {
        for (unsigned int i = 0; i<nb_pages; i++) 
            node_copy(page_owners + i, &me);
    }
}

void clean_data_transfer() {
    free(page_owners);
}
