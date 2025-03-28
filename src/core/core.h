#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include "../network/network.h"
#include "../utils/utils.h"
#include "network/message.h"
#include "utils/list.h"

extern const struct node_id EMPTY_NODE;
extern struct node_id me;
extern struct core_info *core_info;
extern size_t core_size;

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

//Solution: pas grave le mauvais data owner transmet sa requete au data owner qu'elle a enregistrer et une fois que sa requete a été transmis le veritable data owner peut lui envoyer son id pour corriger

// chaque node doit etre a tout moment capable de reorienter les requettes quelle reçoie ou de les traiter

enum requests_status { PENDING, RUNNING };

enum lock_type { READ, WRITE };
 enum lock_status {
	READING = READ,
	WRITING = WRITE,
	NONE
};
struct core_info {
	enum lock_status mode;
	struct node_id write_request;
	struct node_list read_request;
	struct node_id have_token;
};
struct page {
	struct node_id *
		data_owner; // Update lors du changement downer par un broadcast NULL if we own it
	struct node_id *id_lock_given; // NULL if given to none
	// read_requests: Queue<Request>;
	enum requests_status read_requests_status;
	// write_requests: Request?;
	char have_token; // boolean
	// enum lock_status my_lock;
	char in_chainon; //boolean
};

/// TABLEAU a taille fix ou pas ? Difficulter pour l'agrandissement dynamique de la ram => pas demander dans le projet pour l'instant
/// mais es ce vraiment un cas d'utilisation ???
extern struct page *page_info;

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
extern void ask_lock(size_t page_id, enum lock_type lock_type);

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
extern void unlock(size_t page_id,  enum lock_type lock_type);

extern void init_core(size_t nb_pages, void *pages_data);

extern void clean_core(void);

extern int check_core_info_test(void);

extern void *get_core_info(size_t *sz);

extern struct node_id* get_owner(size_t page_id);

extern void set_owner(struct node_id *owner, size_t page_id);