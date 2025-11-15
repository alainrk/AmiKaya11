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

/********************************* MSG.C *******************************

	This module implements the functions necessary for the phase2 kernel
	for the message passing service.

************************************************************************/


#include <const11.h>
#include <const.h>
#include <types11.h>
#include <listx.h>
#include <tcb.e>

/* Sentinel for the free message list */
struct list_head msgfree_sentinel;

/* Array of messages to allocate */
HIDDEN msg_t msg_array[MAXMESSAGES];


/* Message list handling functions */

/* Initializes the free message list and allocates using msg_array with the m_next linker */
void initMsg(void){
	
	int i;
	INIT_LIST_HEAD(&(msgfree_sentinel));	
	for (i=0;i<MAXMESSAGES;i++){	
	list_add_tail(&(msg_array[i].m_next),&(msgfree_sentinel));	
	}
}


/* Moves a message to the free list */
void freeMsg(msg_t *m){
	list_add_tail(&(m->m_next),&(msgfree_sentinel));
}


/* Allocates and initializes the fields of a message */
msg_t *allocMsg(void){
	msg_t *removed_msg;
	
	if (list_empty(&msgfree_sentinel)) return NULL;

	removed_msg=container_of(msgfree_sentinel.next, msg_t, m_next); 
	list_del(&(removed_msg->m_next));
	removed_msg->m_sender=NULL;
	removed_msg->m_message=0;
	/*removed_msg->m_SSIpayload=NULL;*/
	return removed_msg;
}

/* Initializes an empty message queue */
void mkEmptyMessageQ(struct list_head *emptylist){
	INIT_LIST_HEAD(emptylist);
}


/* Checks if the queue is empty */
int emptyMessageQ(struct list_head *head){
	return (list_empty(head));
}

/* Inserts the message pointed to by mp at the tail of the queue with sentinel head */
void insertMessage(struct list_head *head, msg_t *mp){
	list_add_tail(&(mp->m_next),head);
}

/* Inserts at the head (push) the message pointed to by mp in the list with sentinel head */
void pushMessage(struct list_head *head, msg_t *mp){
	list_add(&(mp->m_next),head);
	}

/* Pops the message pointed to by mp from the list with sentinel head, if it exists */
/* If NULL is passed as the second parameter, performs a simple pop (remove from head) */
msg_t *popMessage(struct list_head *head, tcb_t *mp){
	msg_t *pos;
	
	if (list_empty(head)) return NULL;
	if (mp == NULL) {	
		pos = container_of(head->next, msg_t, m_next);
		list_del(head->next);
		return pos;
	}

	list_for_each_entry(pos, head, m_next){
			if (pos->m_sender==mp) {
			list_del(&(pos->m_next));
			return pos;
		}
	}
	return NULL;
}

/* Returns the pointer to the first message at the head of the list with sentinel head */
msg_t *headMessage(struct list_head *head){
	if (list_empty(head)) return NULL;
	return container_of(head->next, msg_t, m_next);
}

/* Useful for phase2 */
/* Checks if there is a message in the inbox from mp otherwise returns NULL */
msg_t *thereIsMessage(struct list_head *head, tcb_t *mp){
	msg_t *pos;
	
	if (list_empty(head)) return NULL;

	list_for_each_entry(pos, head, m_next){
			if (pos->m_sender == mp) return pos;
	}
	return NULL;
}

