#include "Naimi_Trehel.h"

static bool token;
static bool requesting;
static bool leaving;
static struct node_id father;
static struct node_id next;
// mutex to manage data race 
static pthread_mutex_t mtx;
static pthread_cond_t cond;

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
    send_message(dst, &msg, sizeof(struct message));
}

static void REQUEST_CS_handler(struct message *message) {
    pthread_mutex_lock(&mtx);
    if (leaving) goto exit;
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
    exit:
    pthread_mutex_unlock(&mtx);
}

static void GET_CS_handler(struct message *message) {
    pthread_mutex_lock(&mtx);
    token = 1;
    pthread_cond_signal(&cond);
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
    while(!token) pthread_cond_wait(&cond, &mtx);
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

static void init_internal_data(const struct node_id *father_init, 
                               bool token_init, bool requesting_init) {
    token = token_init;
    requesting = requesting_init;
    leaving = 0;
    node_copy(&father, father_init);
    node_copy(&next, &EMPTY_NODE);
}

static void RESET_CS_handler(struct message *message) {
    pthread_mutex_lock(&mtx);
    // we reset the internal data as if we called INIT_CS for the first time
    init_internal_data(&message->sender, 0, requesting);
    if (requesting) request_CS();
    pthread_mutex_unlock(&mtx);
}

static void NEW_ROOT_CS_handler(struct message *message) {
    pthread_mutex_lock(&mtx);
    init_internal_data(&EMPTY_NODE, 1, requesting);
    if (requesting) {
        pthread_cond_signal(&cond);
    }
    struct message msg = {.message_type = RESET_CS};
    broadcast_message(&msg, sizeof(struct message));
    pthread_mutex_unlock(&mtx);
}

void init_CS(const struct node_id *father_init, bool token_init,
             bool requesting_init) {
    init_internal_data(father_init, token_init, requesting_init);
    pthread_mutex_init(&mtx, NULL);
    pthread_cond_init(&cond, NULL);
    addHandler(REQUEST_CS, NULL, REQUEST_CS_handler);
    addHandler(GET_CS, NULL, GET_CS_handler);
    addHandler(RESET_CS, NULL, RESET_CS_handler);
    addHandler(NEW_ROOT_CS, NULL, NEW_ROOT_CS_handler);
}

void clear_CS() {
    pthread_mutex_destroy(&mtx);
    pthread_cond_destroy(&cond);
}

int leave_CS() {
    pthread_mutex_lock(&mtx);
    if (!token) {
        pthread_mutex_unlock(&mtx);
        return -1;
    }
    struct node_id *new_root = NULL;
    size_t sz;
    if (!node_equal(&next, &EMPTY_NODE)) new_root = &next;
    else new_root =  &list_next_entry(&node_list, nlist)->node;

    // there is no ther person in the network
    if (new_root->port == -1) goto exit;
    

    // we just have to inform him, that he is the new root
    struct message msg = {.message_type = NEW_ROOT_CS};
    send_message(new_root, &msg, sizeof(struct message));
    
    exit :
        pthread_mutex_unlock(&mtx);
        clear_CS();
        return 0;

}