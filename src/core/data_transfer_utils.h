#include "data_transfer.h"
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>

static pthread_mutex_t *page_mtx;
static pthread_cond_t *page_cond;
// 1 if we are actually synching the page 0 otherwise
static bool *page_in_transit;
// 1 if the page is up-to-date  otherwise
static bool *page_state;

static void wait_signal(size_t page_id) {
    pthread_mutex_lock(page_mtx + page_id);
    while (page_in_transit[page_id]){
        pthread_cond_wait(page_cond + page_id, page_mtx + page_id);
    }
    pthread_mutex_unlock(page_mtx + page_id);
}

static void signal_page(size_t page_id) {
    pthread_mutex_lock(page_mtx + page_id);
    page_in_transit[page_id] = 0;
    page_state[page_id] = 1;
    pthread_cond_broadcast(page_cond + page_id);
    pthread_mutex_unlock(page_mtx + page_id);
}

static void PAGE_handler(struct message *message) {
    size_t *page_id = (size_t *) (message + 1);
    struct node_id *owner = (struct node_id *) (page_id + 1);
    // np => new page | op => old page
    void *addr_np = (void *) (owner + 1);
    void *addr_op = dsm + (*page_id) * PAGE_SIZE;
    char synching = 0;

    pthread_mutex_lock(page_mtx + *page_id);



    if (message->message_type == RECV_PAGE ) {
        node_copy(page_owners + *page_id, owner);
        memory_unlock_write(*page_id);
        memcpy(addr_op, addr_np, PAGE_SIZE);
        memory_lock_reset(*page_id);
        // if someone is synching we wake him up
        synching = page_in_transit[*page_id];
        pthread_mutex_unlock(page_mtx + *page_id);
        if (synching) signal_page(*page_id);
    }else pthread_mutex_unlock(page_mtx + *page_id);
}

static struct message *build_PAGE_message(size_t page_id, size_t *sz,
                                          struct node_id *owner,
                                          enum message_type recv_type) {
    // contains the page_id, the owner of that page and the page itself
    *sz = sizeof(struct message) + sizeof(size_t) 
                                 + sizeof(struct node_id)
                                 + PAGE_SIZE;
    void *addr_pg = dsm + PAGE_SIZE * page_id;
    struct message *msg = (struct message *) malloc(*sz);
    // set the page_id
    size_t *index_p = (size_t * ) (msg + 1);
    *index_p = page_id;
    // set the new owner
    struct node_id *page_owner = (struct node_id *) (index_p + 1);
    node_copy(page_owner, owner);
    // set the page itself
    memory_unlock_read(page_id);
    memcpy(page_owner + 1, addr_pg, PAGE_SIZE);
    memory_lock_reset(page_id);
    msg->message_type = recv_type;
    return msg;
}

static void RECV_PAGE_handler(struct message *message) {
    PAGE_handler(message);
}

static void transfer_page(struct node_id *requester, size_t page_id) {
    size_t ms_sz;
    struct message *msg = build_PAGE_message(page_id, &ms_sz, &me, RECV_PAGE);
    send_message(requester, msg, ms_sz);
    free_message(msg);
}

// message + id + node
static struct message *build_rqst_message(size_t page_id, struct node_id *node, size_t *sz) {
    // contains a page_id and a node
    *sz =  sizeof(struct message) + sizeof(size_t) 
                                  + sizeof(struct node_id);
    struct message *msg = (struct message *) malloc(*sz);
    size_t * index_p = (size_t *) (msg + 1);
    *index_p = page_id;
    node_copy((struct node_id *) (index_p + 1), node);
    return msg;
}

static void ASK_PAGE_handler(struct message *message) {
    size_t *page_id = (size_t *) (message + 1);
    struct node_id *requester = (struct node_id *) (page_id + 1);

    pthread_mutex_lock(page_mtx + *page_id);
    struct node_id *owner = page_owners + *page_id;
    pthread_mutex_unlock(page_mtx + *page_id);

    if (node_equal(owner, &me)) {
        transfer_page(requester, *page_id);
    }else{
        send_message(owner, message, 
            sizeof(struct message) + sizeof(size_t) + 
            sizeof(struct node_id));
    }
}

static struct message *build_ASK_PAGE_message(size_t page_id, size_t *sz) {
    struct message *msg = build_rqst_message(page_id, &me, sz);
    msg->message_type = ASK_PAGE;
    return msg;
}