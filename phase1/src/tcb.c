/************************************************************************

	Copyright (C) Alain Di Chiappari

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

************************************************************************/

/********************************* TCB.C *******************************

	Questo modulo implementa le funzioni necessarie al nucleo di phase2
	per la gestione delle code e dei thread.

************************************************************************/


#include <const11.h>
#include <const.h>
#include <types11.h>
#include <listx.h>

/*Sentinella per la lista dei thread liberi*/
struct list_head tcbfree_sentinel;

/*Array di thread da allocare*/
HIDDEN tcb_t thread_array[MAXTHREADS];

/*Ausiliaria di allocTcB che inizializza t_state*/
void init_t_state (state_t *state){
	int i=0;

	state->entry_hi=0;
	state->cause=0;
	state->status=0;
	state->pc_epc=0;
	for(i=0; i<29 ; i++){	
		state->gpr[i]=0;
	}

	state->hi=0;
	state->lo=0;
}

/* TCB handling functions */


/*Inizializza la lista dei thread liberi e usando il thread_array alloca col concatenatore t_next*/
void initTcbs(void){
	int i;

	INIT_LIST_HEAD(&(tcbfree_sentinel));

	for (i=0; i<MAXTHREADS; i++){
	list_add_tail(&(thread_array[i].t_next),&(tcbfree_sentinel));	
	}

}
	

/*Sposta un thread nella lista di quelli liberi*/
void freeTcb(tcb_t *p){
	list_add_tail(&(p->t_next),&(tcbfree_sentinel));
	}


/*Alloca e inizializza i campi di un thread restituendo il puntatore ad esso, ottenuto con la macro container_of*/
tcb_t *allocTcb(void){
	tcb_t *removed_tcb;

	if (list_empty(&tcbfree_sentinel)) return NULL;

	removed_tcb=container_of(tcbfree_sentinel.next, tcb_t, t_next);
	list_del(&(removed_tcb->t_next));

	removed_tcb->t_parent = NULL;
	INIT_LIST_HEAD(&(removed_tcb->t_child));
	INIT_LIST_HEAD(&(removed_tcb->t_sib));
	INIT_LIST_HEAD(&(removed_tcb->t_inbox));
	init_t_state(&(removed_tcb->t_state));
	
	removed_tcb->cpu_time = 0;
	removed_tcb->cpu_slice = 0;

	removed_tcb->waiting_for = (tcb_t *)-1;
	removed_tcb->reply = (U32 *)-1;

	removed_tcb->sysbp_manager_thread = NULL;
	removed_tcb->pgmtrap_manager_thread = NULL;
	removed_tcb->tlbtrap_manager_thread = NULL;
	
	return removed_tcb;
}




/*Inizializza un coda di thread vuota*/
void mkEmptyThreadQ(struct list_head *emptylist){
	INIT_LIST_HEAD(emptylist); 
 }

/*Verifica se la coda è vuota*/
int emptyThreadQ(struct list_head *head){
	return (list_empty(head));
}

/*Inserisce in coda il thread puntato da p nella coda a sentinella head*/
void insertThread(struct list_head *head, tcb_t *p){
	list_add_tail(&(p->t_next),head);
}

/*Rimuove un thread in testa alla lista passata, nel caso in cui non sia vuota*/
tcb_t *removeThread(struct list_head *head){
	struct tcb_t *p;
	if (list_empty(head)) return NULL;
	p=container_of(head->next, tcb_t, t_next);
	list_del(head->next);
	return p;
}

/* Rimuove il thread puntato da p dalla lista passata, nel caso ci sia ne restituisce il puntatore, altrimenti NULL */
tcb_t *outThread(struct list_head *head, tcb_t *p){
	tcb_t *pos;
	if (list_empty(head)) return NULL;
	list_for_each_entry(pos, head, t_next) {
		if (pos==p) {
			list_del(&(pos->t_next));
			return pos;
		}
	}
	return NULL;
}


/*Restituisce il puntatore al thread in testa alla lista passata, se non vuota*/
tcb_t *headThread(struct list_head *head){
	if (list_empty(head)) return NULL;
	else return container_of(head->next, tcb_t, t_next);
}



/* Tree view functions */


/*Verifica se il thread puntato da this non ha figli*/
int emptyChild(tcb_t *this){
	return list_empty(&(this->t_child));
}

/*Inserisce il thread puntato da child nella lista dei figli del thread puntato da parent, aggiornando anche il campo t_parent del figlio*/
void insertChild(tcb_t *parent, tcb_t *child){
	child->t_parent = parent;
	list_add_tail(&(child->t_sib),&(parent->t_child));
}

/*Rimuove il figlio in testa tra quelli di parent, nel caso in cui ne abbia*/
tcb_t *removeChild(tcb_t *parent){
	struct tcb_t *pos;	/*Variabile ausialiaria per la restituzione del puntatore a thread dalla macro*/
	struct list_head *t = &(parent->t_child);	/*Variabile ausiliaria per la leggibilità della macro, indica il puntatore alla sentinella della lista dei figli di parent, in questo caso il campo t_child di parent*/
	if (list_empty(&(parent->t_child))) return NULL;
	pos=container_of(t->next, tcb_t, t_sib);
	list_del(&(pos->t_sib));
	pos->t_parent=NULL;	/* Aggiorno il campo t_parent, in quanto non è più suo figlio */
	return pos;
	}

/*Verifica se child ha un padre, nel qual caso rimuove child dai suoi figli*/
tcb_t *outChild(tcb_t *child){
	if (child->t_parent == NULL) return NULL;
	list_del(&(child->t_sib));
	child->t_parent = NULL; /* Aggiorno il campo t_parent */
	return child;
}

/**** Utili per phase 2 ****/

/* Verifica se il thread puntato da p è nella lista passata, nel caso ci sia ne restituisce il puntatore, altrimenti NULL */
tcb_t *thereIsThread(struct list_head *head, tcb_t *p){
	tcb_t *pos;
	if (list_empty(head)) return NULL;
	list_for_each_entry(pos, head, t_next) {
		if (pos==p) return pos;
	}
	return NULL;
}

/*Inserisce il thread new tra i fratelli di sibling */
void insertSibling(tcb_t *sibling, tcb_t *new){
	list_add_tail(&(new->t_sib),&(sibling->t_sib));
}
