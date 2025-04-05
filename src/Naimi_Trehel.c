#include "Naimi_Trehel.h"

bool token;
bool requesting;
struct node_id father;
struct node_id next;
// mutex to manage data race 
static pthread_mutex_t mtx;

static void send_request_to_father(struct node_id *requester) {
    size_t sz = sizeof(struct message) + sizeof(struct node_id);
    struct message *msg = malloc(sz);
    msg->message_type = REQUEST_CS;
    node_copy((struct node_id *) (msg + 1), requester);
    send_message(&father, msg, sz);
    node_copy(&father, &EMPTY_NODE);
    free_message(msg); 
}

static void send_token(struct node_id *dst) {
    struct message msg = {.message_type = GET_CS};
    send_message(&next, &msg, sizeof(struct message));
}

static void REQUEST_CS_handler(struct message *message) {
    pthread_mutex_lock(&mtx);
    struct node_id *requester = (struct node_id *) (message + 1);
    if (node_equal(&father, &EMPTY_NODE)) {
        if (requesting) {
            node_copy(&next, requester);
        }else {
            token = 0;
            send_token(requester);
        }
    }else{
        send_request_to_father(requester);
    }
    node_copy(&father, requester);
    pthread_mutex_unlock(&mtx);
}

static void GET_CS_handler(struct message *message) {
    pthread_mutex_lock(&mtx);
    token = 1;
    pthread_mutex_unlock(&mtx);
}

void request_CS() {
    pthread_mutex_lock(&mtx);
    requesting = 1;
    if (token == 1) {
        pthread_mutex_unlock(&mtx);
        return;
    }
    if (!node_equal(&father, &EMPTY_NODE)) {
        send_request_to_father(&me);
    }
    struct message *msg = wait_message(GET_CS, NULL);
    free_message(msg);
    pthread_mutex_unlock(&mtx);
}

void release_CS() {
    pthread_mutex_lock(&mtx);
    requesting = 0;
    if (!node_equal(&next, &EMPTY_NODE)) {
        send_token(&next);
        token = 0;
        node_copy(&next, &EMPTY_NODE);
    }
    pthread_mutex_unlock(&mtx);
}

void init_CS(struct node_id *father_init, bool token_init) {
    token = token_init;
    requesting = 0;
    pthread_mutex_init(&mtx, NULL);
    node_copy(&father, father_init);
    node_copy(&next, &EMPTY_NODE);
    addHandler(REQUEST_CS, NULL, REQUEST_CS_handler);
    addHandler(GET_CS, NULL, GET_CS_handler);
}

void clear_CS() {
    pthread_mutex_destroy(&mtx);
}