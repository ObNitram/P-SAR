#pragma once

#include <stddef.h>
#include "../network/network.h"

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
    size_t id;
    struct node_id data_owner; // Update lors du changement downer par un broadcast
    struct node_id id_lock_given;
    // read_requests: Queue<Request>;
    enum requests_status read_requests_status;
    // write_requests: Request?;
    char have_token; // boolean
    enum lock_status my_lock;
    char in_chainon; //boolean
};

/// TABLEAU a taille fix ou pas ? Difficulter pour l'agrandissement dynamique de la ram
/// mais es ce vraiment un cas d'utilisation ???
struct page *page_info;


void ask_lock(struct page *page, int lock_type);

void unlock(struct page *page);

void handle_lock_read(struct page *page, struct node_id id_requester);

void handle_unlock_read(struct page * page, int id_requester);

void handle_unlock_write(struct page * page, int id_requester);

void handle_lock_write(struct page * page, int id_requester);