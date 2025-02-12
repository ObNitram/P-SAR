#include "library.h"

#include <stdio.h>


/// Structure global
/// Tableau de tous les read_lock que nous avons pour les pages et la personne qui nous les a envoyés car c'est lui qui
/// a la dernière version de la page.
///
/// Tableau de tous les write_lock que nous avons pour les pages et la personne qui nous les a envoyés car c'est lui qui
/// a la dernière version de la page.

void *InitNode(int size)
{
	/// create shared memory
	/// divide memory into X pages
	/// create X queue : 1 by page
	/// initialise every queue with ourselves in write

	/// wait on a port to allow new node to join us

	return NULL;
}

void *AddNode(char *host)
{
	/// register to system and get shared memory size

	/// create shared memory with the size
	/// lock the shared memory => not necessary but usefull for debug

	return NULL;
}

void lock_read(void *adr, int s)
{
	/// transform adr to page
	/// foreach page in ascending order
	///		lock_read_page(){
	///			look if we have the queue for the page or broadcast to request the lock_read
	///			only the node with the queue answer
	///			wait that someone give us the lock => we save this someone in the global var
	///		}
	///
	/// we have all read_lock.
	/// lock the shared memory => must be already lock but anyway


	/// we setup a handler on sigseg qui va :
	///		regarder que page est toucher
	///		charger la page avec les info de la variable global => si page pas dans la variable global => page pas lock => erreur d'utilisation
	///		unlock the shared memory
	///		go back to the programe
	///
	/// we have setup the handler, we return
}

void unlock_read(void *adr, int s)
{
	/// transform adr to page
	/// we lock all the pages concerned
	/// foreach page in ascending order :
	///		unlock_read_page(){
	///			we notify the node in our global variable that we have finich and that we give back the lock
	///		}
}

void lock_write(void *adr, int s)
{
	/// transform adr to page
	/// foreach page in ascending order
	///		lock_write_page(){
	///			look if we have the queue for the page or broadcast to request the lock_write
	///			only the node with the queue answer
	///			wait that someone give us the lock => he will give us the token, the queue and we save this someone in the global var
	///			we need now to handle all new requet so we setup a thread in the background to handler requet and had them in the queue
	///			we return
	///		}
	///
	/// we have all write_lock.
	/// lock the shared memory => must be already lock but anyway


	/// we setup a handler on sigseg qui va :
	///		regarder que page est toucher
	///		charger la page avec les info de la variable global => si page pas dans la variable global => page pas lock => erreur d'utilisation
	///		unlock the shared memory
	///		go back to the programe
	///
	/// we have setup the handler, we return

}

void unlock_write(void *adr, int s)
{
	/// transform adr to page
	/// we lock all the pages concerned
	/// foreach page in ascending order :
	///		unlock_write_page(){
	///
	///			while(next node is note write){
	///				we send a lock and our identity to all the next read node
	///				we provide listen to them to send theme page if nessesary
	///				we wait that there all finish
	///			}
	///			we send the token to the next node in the queue or we wait than someone else want to do something
	///		}
	///
	///	we no longer have responsibility for any node
	///	we return
}


/// Treading background
/// The node with the tocken broadcast that it have the token
/// Other node wo want right or read send directly to it
/// If the node don't have the token => it redirect the message
/// All read and all write have a different number by node