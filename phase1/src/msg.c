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

	Questo modulo implementa le funzioni necessarie al nucleo di phase2
	per il servizio di message passing.

************************************************************************/


#include <const11.h>
#include <const.h>
#include <types11.h>
#include <listx.h>
#include <tcb.e>

/*Sentinella per la lista dei messaggi liberi*/
struct list_head msgfree_sentinel;

/*Array di messaggi da allocare*/
HIDDEN msg_t msg_array[MAXMESSAGES];


/* Message list handling functions */

/*Inizializza la lista dei messaggi liberi e usando il msg_array alloca col concatenatore m_next*/
void initMsg(void){
	
	int i;
	INIT_LIST_HEAD(&(msgfree_sentinel));	
	for (i=0;i<MAXMESSAGES;i++){	
	list_add_tail(&(msg_array[i].m_next),&(msgfree_sentinel));	
	}
}


/*Sposta un msg nella lista di quelli liberi*/
void freeMsg(msg_t *m){
	list_add_tail(&(m->m_next),&(msgfree_sentinel));
}


/*Alloca e inizializza i campi di un messaggio*/
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

/*Inizializza un coda di msg vuota*/
void mkEmptyMessageQ(struct list_head *emptylist){
	INIT_LIST_HEAD(emptylist);
}


/*Verifica se la coda è vuota*/
int emptyMessageQ(struct list_head *head){
	return (list_empty(head));
}

/*Inserisce in coda il msg puntato da mp nella coda a sentinella head*/
void insertMessage(struct list_head *head, msg_t *mp){
	list_add_tail(&(mp->m_next),head);
}

/*Inserisce in testa (push) il msg puntato da mp nella lista a sentinella head*/
void pushMessage(struct list_head *head, msg_t *mp){
	list_add(&(mp->m_next),head);
	}

/*Fa pop del msg puntato da mp nella lista a sentinella head, nel caso in cui vi sia*/
/*Se viene passato NULL come secondo parametro fa una semplice pop (remove dalla testa)*/
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

/* Restituisce il puntatore al primo messaggio in testa a lista a sentinella head */
msg_t *headMessage(struct list_head *head){
	if (list_empty(head)) return NULL;
	return container_of(head->next, msg_t, m_next);
}

/* Utili per phase2 */
/* Verifica se esiste in inbox un messaggio proveniente da mp altrimenti ritorna NULL */
msg_t *thereIsMessage(struct list_head *head, tcb_t *mp){
	msg_t *pos;
	
	if (list_empty(head)) return NULL;

	list_for_each_entry(pos, head, m_next){
			if (pos->m_sender == mp) return pos;
	}
	return NULL;
}

