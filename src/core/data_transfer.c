#include "data_transfer.h"
#include "../utils/utils.h"
#include "../core/core.h"
#include <stdlib.h>

static void RECV_PAGE_handler(struct message *message) {
    size_t *page_id = (size_t *) (message + 1);
    void *addr_np = (void *) (page_id + 1);
    void *addr_p = dsm + (*page_id) * PAGE_SIZE;
    set_owner(&message->sender, *page_id);
    memcpy(addr_np, addr_p, PAGE_SIZE);
}

static void transfer_page(struct node_id *requester, size_t page_id) {
    void *addr_pg = dsm + PAGE_SIZE * page_id;
    size_t ms_sz = sizeof(struct message) + sizeof(size_t)
                                          + PAGE_SIZE;
    struct message *msg = malloc(ms_sz);
    msg->message_type = RECV_PAGE;
    size_t *index_p = (size_t * ) (msg + 1);
    *index_p = page_id;
    memcpy(index_p + 1, addr_pg, PAGE_SIZE);
    send_message(requester, msg, ms_sz);
    free_message(msg);
}

static void ASK_PAGE_handler(struct message *message) {
    size_t *page_id = (size_t *) (message + 1);
    struct node_id *requester = (struct node_id *) (page_id + 1);
    struct node_id *owner = get_owner(*page_id);
    if (node_equal(owner, &me)) {
        transfer_page(requester, *page_id);
    }else{
        send_message(owner, message, 
            sizeof(struct message) + sizeof(size_t) + 
            sizeof(struct node_id));
    }
}

void sync_page(struct node_id *owner, size_t page_id){
    size_t ms_sz =  sizeof(struct message) + sizeof(size_t) +
                    sizeof(struct node_id);
    struct message *msg = malloc(ms_sz);
    msg->message_type = ASK_PAGE;
    size_t * index_p = (size_t *) (msg + 1);
    *index_p = page_id;
    node_copy((struct node_id *) (index_p + 1), &me);
    send_message(owner, msg, ms_sz);
    free_message(wait_message(RECV_PAGE, NULL));
    free_message(msg);
}

void set_data_transfer_handler() {
    addHandler(RECV_PAGE, NULL, RECV_PAGE_handler);
    addHandler(ASK_PAGE, NULL, ASK_PAGE_handler);
}



