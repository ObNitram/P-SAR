#include "core.h"

struct page page_info;

void init_page(struct page *p, ssize_t id)
{
	p->data_owner = NULL; 
	p->have_token = 1;
	p->id = id;
	p->id_lock_given = NULL;
	p->in_chainon = 1;
	p->my_lock = NONE;
	p->read_requests_status = PENDING;
}

void free_page_info() {
    
}

void ask_lock(struct page *page, int lock_type) {
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
}

void unlock(struct page *page) {
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

