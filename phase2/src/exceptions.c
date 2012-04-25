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

/*************************** EXCEPTIONS.C *******************************

	Questo modulo implementa le routine di gestione delle eccezioni di tipo
	SYSCALL/BREAKPOINT, PROGRAM TRAP, TLB.
	Principalmente gestisce il MESSAGE PASSING sui cui si basa il sistema.
	Inoltre fornisce alcune funzioni utili anche al resto del nucleo.

************************************************************************/


/* Phase1 */
#include <msg.e>
#include <tcb.e>
#include <const11.h>
#include <const.h>
#include <types11.h>
#include <listx.h>
/* Phase2 */
#include <boot.e>
#include <scheduler.e>
#include <interrupts.e>
#include <manager.e>
#include <ssi.e>

/* Macro per leggibilità: riferimento a Old Area come stato del processore */
#define		sysbk_oldarea			((state_t *)SYSBK_OLDAREA)
#define		tlbtrap_oldarea				((state_t *)TLB_OLDAREA)
#define		pgmtrap_oldarea		((state_t *)PGMTRAP_OLDAREA)



/**********************************************************************
														SAVE_STATE

	Funzione ausiliaria per salvare lo stato "source" nello stato "dest"
	passati entrambi come puntatori a t_state.

**********************************************************************/

void save_state(state_t *source, state_t *dest) {
	int i;
	dest->entry_hi = source->entry_hi;
	dest->cause = source->cause;
	dest->status = source->status;
	dest->pc_epc = source->pc_epc;
	dest->hi = source->hi;
	dest->lo = source->lo;
	/* GPR */
	for (i=0;i<=29;i++) {
		dest->gpr[i] = source->gpr[i];
	}
}


/**********************************************************************
														TERMINATE

	Si occupa di:
	-Uccidere root e tutti i figli ricorsivamente
	-Rimettere i thread nella lista dei liberi
	-Togliere i thread dalle varie liste/array in cui sono presenti
	-Decrementare il valore di thread_count
	-Pulire la inbox da eventuali messaggi

**********************************************************************/
void terminate (tcb_t *target) {
	
	msg_t *msg;
	tcb_t *child;

	/* Se ha un padre lo elimino dai suoi figli */
	outChild(target);

	/* Caso ricorsivo -> HA figli (su cui viene chiamata la terminate) */
	while (TRUE) {
		if ( (child = removeChild(target)) == NULL ) break; /* Passa al caso base e pulisce thread, liste, messaggi... */
		terminate(child);
	}

	/* Caso base -> NON ha figli */

	/* Se è in qualche lista o è il thread corrente lo elimino */
	if (((outThread(&ready_queue, target)) != NULL) || (current_thread == target)) {

		/* Pulisco la inbox */
		while (TRUE) {
			if ( (msg = popMessage(&(target->t_inbox), NULL)) == NULL ) break;
			freeMsg(msg);
		}

		/* Se è un manager lo elimino dal trap_managers array */
		delete_manager(target);

		if (current_thread == target) current_thread=NULL;

		/* Restituisco ai thread liberi */
		freeTcb(target);
		thread_count--;
	}

	else if (outThread(&wait_queue, target)) {

		/* Decremento contatore processi in attesa I/O o SSI */	
		if (target->waiting_for == SSI_tcb)
			soft_block_count--;

		/* Tutto come caso precedente*/
		while (TRUE) {
			if ( (msg = popMessage(&(target->t_inbox), NULL)) == NULL ) break;
			freeMsg(msg);
		}

		delete_manager(target);
		freeTcb(target);
		thread_count--;
	}

	else PANIC();
}



/**********************************************************************
														SEND

	Invia un messaggio come richiesto dal sender al destinatario
	specificato nel registro a1 (receiver), col payload specificato
	al registro	a2 (payload).
	Operazione NON BLOCCANTE.

**********************************************************************/
int send (tcb_t *sender, tcb_t *target, U32 payload){

	msg_t *msg;
	tcb_t *_sender_;

	/* Protezione SSI */
	if (sender == SSI_tcb)
		_sender_ = (tcb_t *)MAGIC_SSI;
	else 
		_sender_ = sender;

	/* Destinatario in ready_queue o thread corrente */
	if ( ((thereIsThread(&ready_queue, target)) != NULL ) || (current_thread == target) ) {
		if ((msg = allocMsg()) == NULL) PANIC();
		/* Creazione e compilazione messaggio */

		msg->m_sender = _sender_;
		msg->m_message = payload;

		/* Priorità allo Pseudo Clock Tick -> Messaggio in testa */
		if (_sender_ == (tcb_t*)BUS_INTERVALTIMER)
			pushMessage(&(target->t_inbox), msg);
		/* Trattamento normale -> Messaggio in coda */
		else insertMessage(&(target->t_inbox), msg);

		return (MSGGOOD);
	}
	
	/* Destinatario in wait_queue */
	else if ((thereIsThread(&wait_queue, target)) != NULL ) {

		/* 
				Se sta aspettando un messaggio da questo thread 
				o da chiunque sveglio il thread dest togliendolo 
				dalla wait_queue e lo inserisco nella ready_queue
				passandogli il payload dove mi aveva richiesto e 
				che avevo salvato nel TCB nel	campo "reply".
		*/
		if ((target->waiting_for == _sender_) || (target->waiting_for == ANYMESSAGE)) {
			*(target->reply) = payload;

			/* Risveglio il thread */
			insertThread(&ready_queue, outThread(&wait_queue, target));

			/* Risettaggio campi per message passing */
			target->waiting_for = (tcb_t *)-1;
			target->reply = (U32 *)-1;

			/* Decremento soft block count se caso SSI e restituisco sender al destinatario */
			if (_sender_ == SSI_tcb)
				soft_block_count--;

			target->t_state.reg_v0 = (U32)_sender_;
			
			return (MSGGOOD);
		}

		/* Destinatario momentaneamente in attesa di un altro mittente */
		else {
			/* Tutto come sopra */
			if ((msg = allocMsg()) == NULL) PANIC();

			msg->m_sender = _sender_;
			msg->m_message = payload;

			if (sender == (tcb_t *)BUS_INTERVALTIMER)
				pushMessage(&(target->t_inbox), msg);

			else insertMessage(&(target->t_inbox), msg);

			return (MSGGOOD);
		}

	}

	/* Nessuno dei casi precedenti --> potrebbe essere stato terminato il thread */
	return (MSGNOGOOD);
	
}




/**********************************************************************
														RECV

	Mette in wait per un messaggio (settando prima gli opportuni campi 
	ausiliari nella struttura del tcb richiedente) se non vi è il 
	messaggio cercato nella inbox (o un qualsiasi messaggio per 
	ANYMESSAGE). Caso BLOCCANTE.
	Altrimenti viene creata l'astrazione del messaggio ricevuto 
	restituendo il payload inviato al thread all'indirizzo indicato
	nel registro a2 (reply) ---> CASO NON BLOCCANTE.

**********************************************************************/
tcb_t *recv (tcb_t *receiver, tcb_t *sender, U32 *reply){

	msg_t *msg;

	/* Caso ANYMESSAGE, attesa di un qualsiasi messaggio */
	if (sender == ANYMESSAGE) {
		/* Cerco di estrarre il primo messaggio, se c'è */
		msg = popMessage(&(receiver->t_inbox), NULL);
		
		/* Inbox vuota -> wait */
		if (msg == NULL) {
			/* Per chi sono fermo */
			receiver->waiting_for = ANYMESSAGE;
			/* Dove aspetto la risposta */
			receiver->reply = reply;
			/* Metto in wait_queue */
			insertThread(&wait_queue, receiver);
			current_thread = NULL;

			return NULL;
		}

		/* Inbox NON vuota -> preleva messaggio */
		else {
			*reply = msg->m_message;
			sender = msg->m_sender;
			/* Restituisco il messaggio alla lista dei liberi */
			freeMsg(msg);
			/* Restituisco il mittente del messaggio */
			return(sender);
		}
	}

	/* Caso THREAD SPECIFICATO */
	else if (sender != ANYMESSAGE) {
		/* Cerco di estrarre il messaggio inviato dal thread specificato */
		msg = popMessage(&(receiver->t_inbox), sender);
		
		/* Messaggio non trovato -> wait */
		if (msg == NULL) {
			/* Per chi sono fermo */
			receiver->waiting_for = sender;
			/* Dove aspetto la risposta */
			receiver->reply = reply;
			/* Metto in wait_queue */
			insertThread(&wait_queue, receiver);

			current_thread = NULL;
			return NULL;
		}

		/* Messaggio trovato -> preleva messaggio */
		else {
			*reply = msg->m_message;

			/* Se prelevo risposta ad un servizio decremento soft_block_count */
			if (sender == (tcb_t *)MAGIC_SSI) soft_block_count--;

			/* Restituisco il messaggio alla lista dei liberi */
			freeMsg(msg);
			/* Restituisco il mittente del messaggio */
			return(sender);
		}
	}
	return NULL;
}



/**********************************************************************
												 SYSBP_HANDLER

	Caricato all'arrivo di una eccezione di tipo SYSCALL/BREAKPOINT.
	Fornisce principalmente il servizio di message passing.
	In tutti i casi in cui è sollevata una eccezione diversa da SYSCALL
	di tipo 1 o 2 in Kernel Mode, questa routine, come le 
	altre due di questo modulo affidano il controllo del thread corrente
	ad un Trap Manager, se specificato, altrimenti viene terminato tutto
	il sottoalbero corrispondente.

**********************************************************************/
void sysbp_handler(){

	int exc_cause;
	int KUmode;	/* 0 se Kernel / 1 se User */

	tcb_t *trapped;
	tcb_t *manager;

	tcb_t *sender;
	tcb_t *receiver;

	U32 payload;
	U32 *reply;

	/* Aggiornamento tempo thread */
	current_thread->cpu_slice += (GET_TODLOW - current_thread_tod);
	current_thread->cpu_time += (GET_TODLOW - current_thread_tod);

	/*
		Salvo lo stato del processore del thread che ha sollevato l'eccezione, nel campo t_state del suo tcb.
		Lo stato è salvato nella old area dell'attuale eccezione.
		Modifico opportunamente il PC per non avere un ciclo nel ritorno dall'eccezione.
	*/
	save_state(sysbk_oldarea, &(current_thread->t_state));
	current_thread->t_state.pc_epc += WORD_SIZE;

	/* Recupero il tipo di eccezione avvenuta */
	exc_cause = CAUSE_EXCCODE_GET(sysbk_oldarea->cause);

	/* Controllo se Kernel o User Mode */
	KUmode = (sysbk_oldarea->status & STATUS_KUp) >> 3;
	
	/* Se l'eccezione è una SYSCALL 1 o 2 ed eseguita in kernel mode */
	if (KUmode == 0) {

		if (exc_cause == EXC_SYSCALL) {

			/* MsgSend */
			if (sysbk_oldarea->reg_a0 == SEND) {

				sender = current_thread;
				payload = sysbk_oldarea->reg_a2;
				/* Protezione SSI e incremento thread in attesa di servizio */
				if (sysbk_oldarea->reg_a1 == MAGIC_SSI) {
					receiver = SSI_tcb;
					soft_block_count++;
				}
				else 
					receiver = ((tcb_t *)(sysbk_oldarea->reg_a1));

				/* CASO TRAP MANAGER --> Intercettare messaggio TRAPTERMINATE / TRAPCONTINUE */
				if ((thereIs_manager(sender)) && (receiver->waiting_for == sender)) {

					/*** TRAPTERMINATE ***/
					if (payload == TRAPTERMINATE) {
						trapped = receiver;
						terminate(trapped);	/* Termino thread in wait_queue in attesa della decisione del Trap Manager */

						scheduler();
					}

					/*** TRAPCONTINUE ***/
					if (payload == TRAPCONTINUE) {
						trapped = receiver;
						insertThread(&ready_queue, outThread(&wait_queue, trapped)); /* Altrimenti lo risveglio */

						scheduler();
					}
				}

				/* Caso normale di MsgSend (vedi send) */
				sender->t_state.reg_v0 = send (sender, receiver, payload);

				scheduler();

			}

			/* MsgRecv */
			if (sysbk_oldarea->reg_a0 == RECV) {

				receiver = current_thread;
				reply = (U32 *)sysbk_oldarea->reg_a2;
				/* Protezione SSI (non decremento qui soft block count ma al momento di ricevimento servizio) */
				if (sysbk_oldarea->reg_a1 == MAGIC_SSI)
					sender = SSI_tcb;
				else 
					sender = ((tcb_t *)(sysbk_oldarea->reg_a1));


				/* MsgRecv (vedi recv) e restituisco sender direttamente (nel caso NON BLOCCANTE e non con SSIRequest) */
				current_thread->t_state.reg_v0 = (U32)recv(current_thread, ((tcb_t *)sysbk_oldarea->reg_a1), ((U32 *)sysbk_oldarea->reg_a2));
				
				scheduler();
			}

		}

	}

	
	/* TUTTI gli altri casi*/

	manager = current_thread->sysbp_manager_thread;

	/* Se il thread NON ha specificato un Trap Management thread per questa eccezione viene terminato */
	if (manager == NULL) {
		terminate(current_thread);
		current_thread = NULL;

		scheduler();
	}

	/* Se il thread HA invece specificato il gestore viene freezato */
	else {
		/* Invio messaggio al suo Trap manager con registro cause come payload */
		send(current_thread, manager, sysbk_oldarea->cause);

		/* Setto in TCB chi sto aspettando */
		current_thread->waiting_for = manager;

		/* Freeze del thread in attesa della decisione del manager */
		insertThread(&wait_queue, current_thread);

		current_thread = NULL;

		scheduler();
	}


}



/**********************************************************************
												 PGMTRAP_HANDLER

	Caricato all'arrivo di una eccezione di tipo PGMTRAP.
	Affida il controllo del thread corrente ad un Trap Manager, se 
	specificato, altrimenti viene terminato tutto	il sottoalbero 
	corrispondente.

**********************************************************************/
void pgmtrap_handler(){

	tcb_t *manager;

	/* TUTTE le operazioni sono equivalenti a quelle per SYSBP */
	current_thread->cpu_slice += (GET_TODLOW - current_thread_tod);
	current_thread->cpu_time += (GET_TODLOW - current_thread_tod);

	save_state(pgmtrap_oldarea, &(current_thread->t_state));

	manager = current_thread->pgmtrap_manager_thread;


	if (manager == NULL) {
		terminate(current_thread);
		current_thread = NULL;

		scheduler();
	}

	else {

		send(current_thread, manager, pgmtrap_oldarea->cause);

		current_thread->waiting_for = manager;

		insertThread(&wait_queue, current_thread);

		current_thread = NULL;

		scheduler();
	}

}



/**********************************************************************
												 TLB_HANDLER

	Caricato all'arrivo di una eccezione di tipo TLBTRAP.
	Affida il controllo del thread corrente ad un Trap Manager, se 
	specificato, altrimenti viene terminato tutto	il sottoalbero 
	corrispondente.

**********************************************************************/
void tlb_handler(){

	tcb_t *manager;

	/* TUTTE le operazioni sono equivalenti a quelle per SYSBP */
	current_thread->cpu_slice += (GET_TODLOW - current_thread_tod);
	current_thread->cpu_time += (GET_TODLOW - current_thread_tod);

	save_state(tlbtrap_oldarea, &(current_thread->t_state));

	manager = current_thread->tlbtrap_manager_thread;

	if (manager == NULL) {
		terminate(current_thread);
		current_thread = NULL;

		scheduler();
	}


	else {

		send(current_thread, manager, tlbtrap_oldarea->cause);

		current_thread->waiting_for = manager;

		insertThread(&wait_queue, current_thread);

		current_thread = NULL;

		scheduler();
	}

}

