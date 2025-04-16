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
    // if it's a page that we asked or 
    //the owner of that page that informs us about the new
    // owner, we copy the new owner
    if (message->message_type == RECV_PAGE ||
                node_equal(&message->sender, page_owners + *page_id)) {
        node_copy(page_owners + *page_id, owner);
        memcpy(addr_op, addr_np, PAGE_SIZE);
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
    memcpy(page_owner + 1, addr_pg, PAGE_SIZE);
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

static void ACK_RECV_PAGE_handler(struct message *message) {
    size_t *page_id = (size_t *) (message + 1);
    signal_page(*page_id);
}

static struct message *build_ACK_RECV_PAGE_message(size_t page_id, size_t *sz) {
    *sz = sizeof(struct message) + sizeof(size_t);
    struct message *msg = (struct message *) malloc(*sz);
    size_t *index_p = (size_t *) (msg + 1);
    *index_p = page_id;
    msg->message_type = ACK_RECV_PAGE;
    return msg;
}

static void DT_LEAVE_handler(struct message *message) {
    LOG_DATA_TRANS("new owner defined\n");
    size_t *page_id = (size_t *) (message + 1);
    struct node_id *new_owner = (struct node_id *) (page_id + 1);

    pthread_mutex_lock(page_mtx + *page_id);
    node_copy(page_owners + *page_id, new_owner);
    // we are actually synching this page we emmit a new ASK_PAGE to the
    // right owner
    if (page_in_transit[*page_id]) {
        // just reuse the same message because it's the same structure
        // new_owner now is considered as a requester
        node_copy(new_owner, &me);
        message->message_type = ASK_PAGE;
        send_message(page_owners + *page_id,
                    message,  sizeof(struct message) + 
                              sizeof(size_t) +
                              sizeof(struct node_id));
    }
    pthread_mutex_unlock(page_mtx + *page_id);
    
    // we ACK the change
    size_t ms_sz;
    struct message *msg = build_ACK_RECV_PAGE_message(*page_id, &ms_sz);
    send_message(&message->sender, msg, ms_sz);
    free(msg);

    // we remove the old owner from the node_list
    // lock node_list ?
    free(remove_node(&node_list, &message->sender));   
}

static struct message *build_DT_LEAVE_message(size_t page_id,
                                              struct node_id *new_owner, size_t *sz) {
    struct message *msg = build_rqst_message(page_id, new_owner, sz);
    msg->message_type = DT_LEAVE;
    return msg;
}

static void RECV_PAGE_LEAVE_handler(struct message *message) {
    LOG_DATA_TRANS("someone leaving, i'm new owner\n");
    size_t *page_id = (size_t *) (message + 1);
    PAGE_handler(message);
    // we ACK the changes to the leaver
    size_t ms_sz;
    struct message *msg = build_ACK_RECV_PAGE_message(*page_id, &ms_sz);
    send_message(&message->sender, msg, ms_sz);
    LOG_DATA_TRANS("ACK sent\n");
    // lock node_list
    free(remove_node(&node_list, &message->sender));
    free_message(msg);
}
