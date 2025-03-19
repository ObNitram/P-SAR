#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include "../network/network.h"

#define PAGE_SIZE 4096

// Glossaire:
//     data owner : la node qui à la dernière version d'une page
//     token : Un seul token par page, seul celui qui à le token peut donner des locks
//     Chainon : liste de node qui gère la queue, la queue mininal démarre au data owner et fini au dernier qui à fait une write request.

// Methode pour savoir si on est dans le chainon minimal :
//     - Rentrée dans le chainon : Demande de lock en write
//     - Sortie du chainon : lock laché puis reçoit invalidation

// Alternative au chainon: si je veux lacher un lock en write et que je n'ai pas modifier les données, plutôt que ralongé le chainon, je peux prévenir le data owner du nouveau propriétaire du lock.
// Rajoute un (1) message mais limite strictement la longueur du chainons à un maximum de 2.

// TODO: Si deux invalidation sont envoyé l'une après l'autre et une node les reçois dans le mauvais ordre, elle retient le mauvais data owner.
// Solution: rajouter un int dans le Token, l'incrementer quand on le reçoit et envoyer cette int dans une invalidating. Une node ensuite garde seulement le plus grand des deux


// chaque node doit etre a tout moment capable de reorienter les requettes quelle reçoie ou de les traiter

enum lock_status {
    NONE,
    READ,
    WRITE
};

enum requests_status {
    PENDING,
    RUNNING
};


struct page {
    struct node_id *data_owner; // Update lors du changement downer par un broadcast NULL if we own it
    struct node_id *id_lock_given;// NULL if given to none
    // read_requests: Queue<Request>;
    enum requests_status read_requests_status;
    // write_requests: Request?;
    char have_token; // boolean
    enum lock_status my_lock;
    char in_chainon; //boolean
};

/// @brief A global variable representing the linked list of the intern state of 
/// the allocated memory. The first element is a ghost page allocated in the stack
/// with id = -1.
extern struct page *page_info;

void init_page(struct page *p);

void free_page_info();

void ask_lock(size_t page_id, int lock_type);

void unlock(size_t page_id);

void handle_lock_read(struct page *page, struct node_id id_requester);

void handle_unlock_read(struct page * page, int id_requester);

void handle_unlock_write(struct page * page, int id_requester);

void handle_lock_write(struct page * page, int id_requester);