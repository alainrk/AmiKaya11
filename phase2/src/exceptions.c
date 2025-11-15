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

	This module implements the exception handling routines for
	SYSCALL/BREAKPOINT, PROGRAM TRAP, TLB types.
	It mainly handles MESSAGE PASSING on which the system is based.
	It also provides some functions useful to the rest of the kernel.

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

/* Macro for readability: reference to Old Area as processor state */
#define		sysbk_oldarea			((state_t *)SYSBK_OLDAREA)
#define		tlbtrap_oldarea				((state_t *)TLB_OLDAREA)
#define		pgmtrap_oldarea		((state_t *)PGMTRAP_OLDAREA)



/**********************************************************************
														SAVE_STATE

	Auxiliary function to save the "source" state into the "dest" state
	both passed as pointers to t_state.

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

	Handles:
	-Killing root and all children recursively
	-Returning threads to the free list
	-Removing threads from various lists/arrays where they are present
	-Decrementing the thread_count value
	-Cleaning the inbox from any messages

**********************************************************************/
void terminate (tcb_t *target) {
	
	msg_t *msg;
	tcb_t *child;

	/* If it has a parent, remove it from its children */
	outChild(target);

	/* Recursive case -> HAS children (on which terminate is called) */
	while (TRUE) {
		if ( (child = removeChild(target)) == NULL ) break; /* Move to base case and clean threads, lists, messages... */
		terminate(child);
	}

	/* Base case -> NO children */

	/* If it's in some list or is the current thread, remove it */
	if (((outThread(&ready_queue, target)) != NULL) || (current_thread == target)) {

		/* Clean the inbox */
		while (TRUE) {
			if ( (msg = popMessage(&(target->t_inbox), NULL)) == NULL ) break;
			freeMsg(msg);
		}

		/* If it's a manager, remove it from the trap_managers array */
		delete_manager(target);

		if (current_thread == target) current_thread=NULL;

		/* Return to free threads */
		freeTcb(target);
		thread_count--;
	}

	else if (outThread(&wait_queue, target)) {

		/* Decrement counter of processes waiting for I/O or SSI */
		if (target->waiting_for == SSI_tcb)
			soft_block_count--;

		/* Everything as previous case */
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

	Sends a message as requested by the sender to the recipient
	specified in register a1 (receiver), with the payload specified
	in register a2 (payload).
	NON-BLOCKING operation.

**********************************************************************/
int send (tcb_t *sender, tcb_t *target, U32 payload){

	msg_t *msg;
	tcb_t *_sender_;

	/* SSI protection */
	if (sender == SSI_tcb)
		_sender_ = (tcb_t *)MAGIC_SSI;
	else
		_sender_ = sender;

	/* Recipient in ready_queue or current thread */
	if ( ((thereIsThread(&ready_queue, target)) != NULL ) || (current_thread == target) ) {
		if ((msg = allocMsg()) == NULL) PANIC();
		/* Message creation and compilation */

		msg->m_sender = _sender_;
		msg->m_message = payload;

		/* Priority to Pseudo Clock Tick -> Message at head */
		if (_sender_ == (tcb_t*)BUS_INTERVALTIMER)
			pushMessage(&(target->t_inbox), msg);
		/* Normal treatment -> Message at tail */
		else insertMessage(&(target->t_inbox), msg);

		return (MSGGOOD);
	}

	/* Recipient in wait_queue */
	else if ((thereIsThread(&wait_queue, target)) != NULL ) {

		/*
				If it's waiting for a message from this thread
				or from anyone, wake up the dest thread by removing it
				from wait_queue and insert it into ready_queue
				passing it the payload where it requested and
				that I had saved in the TCB in the "reply" field.
		*/
		if ((target->waiting_for == _sender_) || (target->waiting_for == ANYMESSAGE)) {
			*(target->reply) = payload;

			/* Wake up the thread */
			insertThread(&ready_queue, outThread(&wait_queue, target));

			/* Reset fields for message passing */
			target->waiting_for = (tcb_t *)-1;
			target->reply = (U32 *)-1;

			/* Decrement soft block count if SSI case and return sender to recipient */
			if (_sender_ == SSI_tcb)
				soft_block_count--;

			target->t_state.reg_v0 = (U32)_sender_;

			return (MSGGOOD);
		}

		/* Recipient temporarily waiting for another sender */
		else {
			/* Everything as above */
			if ((msg = allocMsg()) == NULL) PANIC();

			msg->m_sender = _sender_;
			msg->m_message = payload;

			if (sender == (tcb_t *)BUS_INTERVALTIMER)
				pushMessage(&(target->t_inbox), msg);

			else insertMessage(&(target->t_inbox), msg);

			return (MSGGOOD);
		}

	}

	/* None of the previous cases --> the thread might have been terminated */
	return (MSGNOGOOD);
	
}




/**********************************************************************
														RECV

	Puts in wait for a message (first setting the appropriate auxiliary
	fields in the requesting tcb structure) if the searched message is
	not in the inbox (or any message for ANYMESSAGE). BLOCKING case.
	Otherwise the abstraction of the received message is created
	returning the payload sent to the thread at the address indicated
	in register a2 (reply) ---> NON-BLOCKING CASE.

**********************************************************************/
tcb_t *recv (tcb_t *receiver, tcb_t *sender, U32 *reply){

	msg_t *msg;

	/* ANYMESSAGE case, waiting for any message */
	if (sender == ANYMESSAGE) {
		/* Try to extract the first message, if there is one */
		msg = popMessage(&(receiver->t_inbox), NULL);

		/* Inbox empty -> wait */
		if (msg == NULL) {
			/* Who I'm waiting for */
			receiver->waiting_for = ANYMESSAGE;
			/* Where I expect the response */
			receiver->reply = reply;
			/* Put in wait_queue */
			insertThread(&wait_queue, receiver);
			current_thread = NULL;

			return NULL;
		}

		/* Inbox NOT empty -> retrieve message */
		else {
			*reply = msg->m_message;
			sender = msg->m_sender;
			/* Return the message to the free list */
			freeMsg(msg);
			/* Return the message sender */
			return(sender);
		}
	}

	/* SPECIFIED THREAD case */
	else if (sender != ANYMESSAGE) {
		/* Try to extract the message sent by the specified thread */
		msg = popMessage(&(receiver->t_inbox), sender);

		/* Message not found -> wait */
		if (msg == NULL) {
			/* Who I'm waiting for */
			receiver->waiting_for = sender;
			/* Where I expect the response */
			receiver->reply = reply;
			/* Put in wait_queue */
			insertThread(&wait_queue, receiver);

			current_thread = NULL;
			return NULL;
		}

		/* Message found -> retrieve message */
		else {
			*reply = msg->m_message;

			/* If I retrieve response to a service, decrement soft_block_count */
			if (sender == (tcb_t *)MAGIC_SSI) soft_block_count--;

			/* Return the message to the free list */
			freeMsg(msg);
			/* Return the message sender */
			return(sender);
		}
	}
	return NULL;
}



/**********************************************************************
												 SYSBP_HANDLER

	Loaded upon arrival of a SYSCALL/BREAKPOINT type exception.
	Mainly provides the message passing service.
	In all cases where an exception other than SYSCALL type 1 or 2
	is raised in Kernel Mode, this routine, like the other two in this
	module, hands over control of the current thread to a Trap Manager,
	if specified, otherwise the entire corresponding subtree is terminated.

**********************************************************************/
void sysbp_handler(){

	int exc_cause;
	int KUmode;	/* 0 if Kernel / 1 if User */

	tcb_t *trapped;
	tcb_t *manager;

	tcb_t *sender;
	tcb_t *receiver;

	U32 payload;
	U32 *reply;

	/* Thread time update */
	current_thread->cpu_slice += (GET_TODLOW - current_thread_tod);
	current_thread->cpu_time += (GET_TODLOW - current_thread_tod);

	/*
		Save the processor state of the thread that raised the exception, in the t_state field of its tcb.
		The state is saved in the old area of the current exception.
		Modify the PC appropriately to avoid a loop when returning from the exception.
	*/
	save_state(sysbk_oldarea, &(current_thread->t_state));
	current_thread->t_state.pc_epc += WORD_SIZE;

	/* Retrieve the type of exception that occurred */
	exc_cause = CAUSE_EXCCODE_GET(sysbk_oldarea->cause);

	/* Check if Kernel or User Mode */
	KUmode = (sysbk_oldarea->status & STATUS_KUp) >> 3;

	/* If the exception is a SYSCALL 1 or 2 and executed in kernel mode */
	if (KUmode == 0) {

		if (exc_cause == EXC_SYSCALL) {

			/* MsgSend */
			if (sysbk_oldarea->reg_a0 == SEND) {

				sender = current_thread;
				payload = sysbk_oldarea->reg_a2;
				/* SSI protection and increment threads waiting for service */
				if (sysbk_oldarea->reg_a1 == MAGIC_SSI) {
					receiver = SSI_tcb;
					soft_block_count++;
				}
				else
					receiver = ((tcb_t *)(sysbk_oldarea->reg_a1));

				/* TRAP MANAGER CASE --> Intercept TRAPTERMINATE / TRAPCONTINUE message */
				if ((thereIs_manager(sender)) && (receiver->waiting_for == sender)) {

					/*** TRAPTERMINATE ***/
					if (payload == TRAPTERMINATE) {
						trapped = receiver;
						terminate(trapped);	/* Terminate thread in wait_queue waiting for Trap Manager decision */

						scheduler();
					}

					/*** TRAPCONTINUE ***/
					if (payload == TRAPCONTINUE) {
						trapped = receiver;
						insertThread(&ready_queue, outThread(&wait_queue, trapped)); /* Otherwise wake it up */

						scheduler();
					}
				}

				/* Normal MsgSend case (see send) */
				sender->t_state.reg_v0 = send (sender, receiver, payload);

				scheduler();

			}

			/* MsgRecv */
			if (sysbk_oldarea->reg_a0 == RECV) {

				receiver = current_thread;
				reply = (U32 *)sysbk_oldarea->reg_a2;
				/* SSI protection (don't decrement soft block count here but when receiving service) */
				if (sysbk_oldarea->reg_a1 == MAGIC_SSI)
					sender = SSI_tcb;
				else
					sender = ((tcb_t *)(sysbk_oldarea->reg_a1));


				/* MsgRecv (see recv) and return sender directly (in NON-BLOCKING case and not with SSIRequest) */
				current_thread->t_state.reg_v0 = (U32)recv(current_thread, ((tcb_t *)sysbk_oldarea->reg_a1), ((U32 *)sysbk_oldarea->reg_a2));
				
				scheduler();
			}

		}

	}


	/* ALL other cases */

	manager = current_thread->sysbp_manager_thread;

	/* If the thread has NOT specified a Trap Management thread for this exception, it is terminated */
	if (manager == NULL) {
		terminate(current_thread);
		current_thread = NULL;

		scheduler();
	}

	/* If the thread HAS instead specified the handler, it is frozen */
	else {
		/* Send message to its Trap manager with cause register as payload */
		send(current_thread, manager, sysbk_oldarea->cause);

		/* Set in TCB who I'm waiting for */
		current_thread->waiting_for = manager;

		/* Freeze the thread waiting for manager decision */
		insertThread(&wait_queue, current_thread);

		current_thread = NULL;

		scheduler();
	}


}



/**********************************************************************
												 PGMTRAP_HANDLER

	Loaded upon arrival of a PGMTRAP type exception.
	Hands over control of the current thread to a Trap Manager, if
	specified, otherwise the entire corresponding subtree is terminated.

**********************************************************************/
void pgmtrap_handler(){

	tcb_t *manager;

	/* ALL operations are equivalent to those for SYSBP */
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

	Loaded upon arrival of a TLBTRAP type exception.
	Hands over control of the current thread to a Trap Manager, if
	specified, otherwise the entire corresponding subtree is terminated.

**********************************************************************/
void tlb_handler(){

	tcb_t *manager;

	/* ALL operations are equivalent to those for SYSBP */
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

