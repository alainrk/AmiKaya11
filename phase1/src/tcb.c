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

	This module implements the functions necessary for the phase2 kernel
	for managing queues and threads.

************************************************************************/


#include <const11.h>
#include <const.h>
#include <types11.h>
#include <listx.h>

/* Sentinel for the free thread list */
struct list_head tcbfree_sentinel;

/* Array of threads to allocate */
HIDDEN tcb_t thread_array[MAXTHREADS];

/* Auxiliary function for allocTcb that initializes t_state */
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


/* Initializes the free thread list and allocates using thread_array with the t_next linker */
void initTcbs(void){
	int i;

	INIT_LIST_HEAD(&(tcbfree_sentinel));

	for (i=0; i<MAXTHREADS; i++){
	list_add_tail(&(thread_array[i].t_next),&(tcbfree_sentinel));	
	}

}
	

/* Moves a thread to the free list */
void freeTcb(tcb_t *p){
	list_add_tail(&(p->t_next),&(tcbfree_sentinel));
	}


/* Allocates and initializes the fields of a thread, returning a pointer to it, obtained with the container_of macro */
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




/* Initializes an empty thread queue */
void mkEmptyThreadQ(struct list_head *emptylist){
	INIT_LIST_HEAD(emptylist); 
 }

/* Checks if the queue is empty */
int emptyThreadQ(struct list_head *head){
	return (list_empty(head));
}

/* Inserts the thread pointed to by p at the tail of the queue with sentinel head */
void insertThread(struct list_head *head, tcb_t *p){
	list_add_tail(&(p->t_next),head);
}

/* Removes a thread from the head of the passed list, if it is not empty */
tcb_t *removeThread(struct list_head *head){
	struct tcb_t *p;
	if (list_empty(head)) return NULL;
	p=container_of(head->next, tcb_t, t_next);
	list_del(head->next);
	return p;
}

/* Removes the thread pointed to by p from the passed list, if it exists returns the pointer, otherwise NULL */
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


/* Returns the pointer to the thread at the head of the passed list, if not empty */
tcb_t *headThread(struct list_head *head){
	if (list_empty(head)) return NULL;
	else return container_of(head->next, tcb_t, t_next);
}



/* Tree view functions */


/* Checks if the thread pointed to by this has no children */
int emptyChild(tcb_t *this){
	return list_empty(&(this->t_child));
}

/* Inserts the thread pointed to by child in the children list of the thread pointed to by parent, also updating the t_parent field of the child */
void insertChild(tcb_t *parent, tcb_t *child){
	child->t_parent = parent;
	list_add_tail(&(child->t_sib),&(parent->t_child));
}

/* Removes the first child from parent's children, if it has any */
tcb_t *removeChild(tcb_t *parent){
	struct tcb_t *pos;	/* Auxiliary variable for returning the thread pointer from the macro */
	struct list_head *t = &(parent->t_child);	/* Auxiliary variable for macro readability, indicates the pointer to the sentinel of parent's children list, in this case the t_child field of parent */
	if (list_empty(&(parent->t_child))) return NULL;
	pos=container_of(t->next, tcb_t, t_sib);
	list_del(&(pos->t_sib));
	pos->t_parent=NULL;	/* Update the t_parent field, as it is no longer its child */
	return pos;
	}

/* Checks if child has a parent, in which case removes child from its children */
tcb_t *outChild(tcb_t *child){
	if (child->t_parent == NULL) return NULL;
	list_del(&(child->t_sib));
	child->t_parent = NULL; /* Update the t_parent field */
	return child;
}

/**** Useful for phase 2 ****/

/* Checks if the thread pointed to by p is in the passed list, if it exists returns the pointer, otherwise NULL */
tcb_t *thereIsThread(struct list_head *head, tcb_t *p){
	tcb_t *pos;
	if (list_empty(head)) return NULL;
	list_for_each_entry(pos, head, t_next) {
		if (pos==p) return pos;
	}
	return NULL;
}

/* Inserts the thread new among the siblings of sibling */
void insertSibling(tcb_t *sibling, tcb_t *new){
	list_add_tail(&(new->t_sib),&(sibling->t_sib));
}
