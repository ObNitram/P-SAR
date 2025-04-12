#include "data_transfer.h"
#include "../utils/utils.h"
#include "../core/core.h"
#include <stdlib.h>
#include <pthread.h>

struct node_id *page_owners;
static pthread_mutex_t *page_mtx;
static pthread_cond_t *page_cond;
// represents the state of the page, 1 if we are actually synching it else 0
static char *page_state;

static void wait_page(size_t page_id) {
    pthread_mutex_lock(page_mtx + page_id);
    page_state[page_id] = 1;
    do {
        pthread_cond_wait(page_cond + page_id, page_mtx + page_id);
    } while (page_state[page_id]);
    pthread_mutex_unlock(page_mtx + page_id);
}

static void signal_page(size_t page_id) {
    page_state[page_id] = 0;
    pthread_cond_signal(page_cond + page_id);
}

static void RECV_PAGE_handler(struct message *message) {
    size_t *page_id = (size_t *) (message + 1);
    void *addr_np = (void *) (page_id + 1);
    void *addr_p = dsm + (*page_id) * PAGE_SIZE;
    pthread_mutex_lock(page_mtx + *page_id);
    node_copy(page_owners + *page_id, &message->sender);
    signal_page(*page_id);
    memcpy(addr_p, addr_np, PAGE_SIZE);
    pthread_mutex_unlock(page_mtx + *page_id);
}

static struct message *build_RECV_PAGE_message(size_t page_id, size_t *sz) {
    void *addr_pg = dsm + PAGE_SIZE * page_id;
    *sz = sizeof(struct message) + sizeof(size_t)
                                          + PAGE_SIZE;
    struct message *msg = malloc(*sz);
    msg->message_type = RECV_PAGE;
    size_t *index_p = (size_t * ) (msg + 1);
    *index_p = page_id;
    memcpy(index_p + 1, addr_pg, PAGE_SIZE);
    return msg;
}

static void transfer_page(struct node_id *requester, size_t page_id) {
    size_t ms_sz;
    struct message *msg = build_RECV_PAGE_message(page_id, &ms_sz);
    send_message(requester, msg, ms_sz);
    free_message(msg);
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
    *sz =  sizeof(struct message) + sizeof(size_t) +
                    sizeof(struct node_id);
    struct message *msg = malloc(*sz);
    msg->message_type = ASK_PAGE;
    size_t * index_p = (size_t *) (msg + 1);
    *index_p = page_id;
    node_copy((struct node_id *) (index_p + 1), &me);
    return msg;
}

void set_new_owner(size_t page_id, struct node_id *new_owner) {
    pthread_mutex_lock(page_mtx + page_id);
    node_copy(page_owners + page_id, new_owner);
    pthread_mutex_unlock(page_mtx + page_id);
}

void sync_page(struct node_id *owner, size_t page_id){
    pthread_mutex_lock(page_mtx + page_id);
    if (node_equal(page_owners + page_id, &me)) {
        pthread_mutex_unlock(page_mtx + page_id);
        return;
    }
    pthread_mutex_unlock(page_mtx + page_id);
    size_t ms_sz;
    struct message *msg = build_ASK_PAGE_message(page_id, &ms_sz);
    send_message(owner, msg, ms_sz);
    wait_page(page_id);
    LOG_DATA_TRANS("synced page %zu\n", page_id);
    free_message(msg);
}

void init_data_transfer(unsigned int nb_pages, struct node_id* owners) {
    addHandler(RECV_PAGE, NULL, RECV_PAGE_handler);
    addHandler(ASK_PAGE, NULL, ASK_PAGE_handler);
    page_owners = malloc(nb_pages * sizeof(struct node_id));
    page_mtx = malloc(nb_pages * sizeof(pthread_mutex_t));
    page_cond = malloc(nb_pages * sizeof(pthread_cond_t));
    page_state = malloc(nb_pages * sizeof(char));
    for (unsigned int i = 0; i < nb_pages; i++) {
        pthread_mutex_init(page_mtx + i, NULL);
        pthread_cond_init(page_cond + i, NULL);
        page_state[i] = 0;
        
    }
    if (owners) {
        memcpy(page_owners, owners, sizeof(struct node_id) * nb_pages);
    }else {
        for (unsigned int i = 0; i<nb_pages; i++) 
            node_copy(page_owners + i, &me);
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
