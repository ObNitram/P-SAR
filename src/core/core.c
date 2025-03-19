#include "core.h"
#include "../utils/message.h"
#include "network/network.h"
#include "utils/list.h"
#include <stdlib.h>

// enum for local status of lock
enum lock_status {
    READING = READ,
    WRITING = WRITE,
    NONE,
};

struct read_request {
    struct node_id node;
    struct list_head list;
};

struct core_info {
    enum lock_status mode;
    struct node_id write_request;
    struct read_request read_request;
    struct node_id have_token;
};

static struct core_info *core_info;
static size_t core_size;

struct slsm_message {
    enum message_type type;
    struct node_id sender; // the sender of the message
    enum lock_type mode;
    struct node_id test; // the sender of the request
};

void init_core(size_t nbpages){
    //create structure sauf si dans page_info
    core_info = malloc(sizeof(struct core_info) * nbpages);
    core_size = nbpages;
    //init handler
}

void clean_core(){
    free(core_info);
    core_info = NULL;
}

void ask_lock(size_t page_id, enum lock_type lock_type) {
    // if (page->owner == id) {
    //     // TODO
    // } else {
    //     send(page->owner, ASK_OWNER, <id_page, my_id, lock_type>);
    //     wait(ACK_LOCK); // on est maintenant dans la file d'attente
    //     // TODO; deal with negative ack
    //     wait(LOCK_GIVEN; any);
    // }
    // page->my_lock = lock_type;
    // page->id_lock_given_from = lock_giver;
    // if (lock_type == WRITE) {
    //     page->have_token = true;
    // }

    struct core_info working_page = core_info[page_id];
    working_page.mode = (enum lock_status)lock_type;

    struct slsm_message request;
    struct slsm_message *response;
    request.test = working_page.have_token;
    request.mode = lock_type;

    send(ASK_LOCK, &working_page.have_token, &request, sizeof(struct slsm_message));
    response = wait(GET_LOCK, NULL);

    switch (working_page.mode) {
    case WRITE:
        working_page.have_token = 
        break;
    case READ:
        break;
    default:
        
        break;
    }
}

void unlock(size_t page_id) {
    // assert(page->my_lock != NONE);
    // if (page->my_lock == READ) {  // we readed
    //     assert(read_request.empty()); 
    //     assert(write_request == NULL);
    // send message back to the guy with the tocken
    //     // if (page->id_given_lock_from == my_id) {
    //     handle
    //     } else {
    //             send(page->given_lock_from, UNLOCK, <page_id, my_id, lock_type>);
    //     }
    // } else {
    //     handle_pending_request(page); // gerer les prochains read et gerer prochain right
    // }
    // page->my_lock = NONE;
}

void handle_ASK_LOCK(struct message *message){
    struct slsm_message request = *((struct slsm_message*) message);
    assert(request.type == ASK_LOCK);
}

void handle_GET_LOCK(struct message *message){
    struct slsm_message request = *((struct slsm_message*) message);
    assert(request.type == GET_LOCK);
}

void handle_UNLOCK(struct message *message){
    struct slsm_message request = *((struct slsm_message*) message);
    assert(request.type == UNLOCK);
}

void handle_lock_read(struct page * page, struct node_id id_requester) {
    // if (page->in_chainon == false) {
    //     send(page->data_owner, ASK_LOCK, <id_page, id_requester, READ>);
    // }
    // if (page->write_request != NULL) {
    //     if (page->write_request == me) {
    //         // Who knows
    //     }
    //     send(page_write_request->id, ASK_LOCK, <id_page, id_requester, READ>);
    //     return;
    // } else {
    //     page->read_request.insert(id_requester);
    //     send(id_requester, ACK_LOCK, <id_page, my_id, READ>);
    //     if (page->have_token && page->my_lock != WRITE) {
    //         send(id_requester, GIVEN_LOCK, <id_page, my_id, READ>);
    //     }
    // }
}

void handle_unlock_read(struct page * page, int id_requester) {
    // page->read_request, id_request);
    // if (page->write_request != NULL && page->read-request.empty()) {
    //     page->have_token = false;
    //     send(page->write_request, GIVEN_LOCK, <id_page, my_id, WRITE>);
    // }
}

void handle_lock_write(struct page* p, int id_requester) {
}
void handle_unlock_write(struct page * p, int id_requester) {
}

