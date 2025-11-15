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

/****************************** SSI.C ***********************************

	This module implements the System Service Interface, which provides a
	real interface to the outside of the kernel, and at the same time
	communicates with it for the management of Pseudo Clock Tick and
	interrupts.

************************************************************************/



/* Phase1 */
#include <msg.e>
#include <tcb.e>
/* Phase2 */
#include <boot.e>
#include <scheduler.e>
#include <interrupts.e>
#include <exceptions.e>
#include <manager.e>



/**********************************************************************
												 REST_INDEX

	Assigns an index to a Device Register address used
	in the arrays used by SSI for interrupt/WaitforIO management.

**********************************************************************/
HIDDEN int rest_index (memaddr dev) {

	if (dev == 0x10000050) return 0;
	if (dev == 0x10000060) return 1;
	if (dev == 0x10000070) return 2;
	if (dev == 0x10000080) return 3;
	if (dev == 0x10000090) return 4;
	if (dev == 0x100000a0) return 5;
	if (dev == 0x100000b0) return 6;
	if (dev == 0x100000c0) return 7;
	if (dev == 0x100000d0) return 8;
	if (dev == 0x100000e0) return 9;
	if (dev == 0x100000f0) return 10;
	if (dev == 0x10000100) return 11;
	if (dev == 0x10000110) return 12;
	if (dev == 0x10000120) return 13;
	if (dev == 0x10000130) return 14;
	if (dev == 0x10000140) return 15;
	if (dev == 0x10000150) return 16;
	if (dev == 0x10000160) return 17;
	if (dev == 0x10000170) return 18;
	if (dev == 0x10000180) return 19;
	if (dev == 0x10000190) return 20;
	if (dev == 0x100001a0) return 21;
	if (dev == 0x100001b0) return 22;
	if (dev == 0x100001c0) return 23;
	if (dev == 0x100001d0) return 24;
	if (dev == 0x100001e0) return 25;
	if (dev == 0x100001f0) return 26;
	if (dev == 0x10000200) return 27;
	if (dev == 0x10000210) return 28;
	if (dev == 0x10000220) return 29;
	if (dev == 0x10000230) return 30;
	if (dev == 0x10000240) return 31;
	if (dev == 0x10000250) return 32;					/* Terminal 0 RECV_STATUS */
	if (dev == 0x10000250 + 0x8) return 33;		/* Terminal 0 TRANSM_STATUS */
	if (dev == 0x10000260) return 34;
	if (dev == 0x10000260 + 0x8) return 35;
	if (dev == 0x10000270) return 36;
	if (dev == 0x10000270 + 0x8) return 37;
	if (dev == 0x10000280) return 38;
	if (dev == 0x10000280 + 0x8) return 39;		/*    ""           ""     */
	if (dev == 0x10000290) return 40;
	if (dev == 0x10000290 + 0x8) return 41;		/*    ""           ""     */
	if (dev == 0x100002a0) return 42;
	if (dev == 0x100002a0 + 0x8) return 43;
	if (dev == 0x100002b0) return 44;
	if (dev == 0x100002b0 + 0x8) return 45;
	if (dev == 0x100002c0) return 46;					/* Terminal 7 RECV_STATUS */
	if (dev == 0x100002c0 + 0x8) return 47;		/* Terminal 7 TRANSM_STATUS */
	else return (-1);
}

/*
	Array where a status field of the corresponding Device Register
	will be stored if a corresponding WAITFORIO has not yet arrived.
*/
HIDDEN int devStatus_array[48];

/*
	Array where a pointer to TCB will be stored for the thread that has
	already done WAITFORIO on a Device from which Device Register has not
	yet received an interrupt.
*/
HIDDEN tcb_t *devTcb_array[48];

/*
	Array for Pseudo Clock management.
	It will store pointers to TCB of those threads that have
	requested the WAITFORCLOCK service.
*/
HIDDEN tcb_t *clockTcb_array[MAXTHREADS];


/*
	Structure useful for sending request message to SSI
	as it maps in the single "U32 payload" field of MsgSend
	the 3 fields service, payload, reply of SSIRequest.

	The SSI will therefore have to handle a message inside which
	there will be a structure made this way, and use the
	information contained in it to provide the service.
*/
struct SSI_request_msg {
	U32 service;		/* Requested service */
	U32 arg;				/* Optional parameter */
	U32 *reply; 		/* Where the response will be placed */
};



/**********************************************************************
										      	SSIREQUEST

	SSIRequest implemented as a combination of:
	MsgSend ---> Sends request type to SSI as message.
	MsgRecv ---> Waits (blocking) for response from SSI.

	The use of SSI_request_msg structures is fundamental.

**********************************************************************/
void SSIRequest (U32 service, U32 payload, U32 *reply) {

	/* Create structure for message sending */
	struct SSI_request_msg request;

	/* Field mapping */
	request.service = service;
	request.arg = payload; /* Could be 0 if there are none */
	request.reply = reply;

	/* Send message with address of the just created structure */
	MsgSend(SEND,MAGIC_SSI,&request);

	/* Wait as a normal message */
	MsgRecv(RECV,MAGIC_SSI,reply);

}



/**********************************************************************
										      	SSI_THREAD

	The System Service Interface is a thread that works similar to
	an RPC server, waiting for a request to arrive and acting
	accordingly, setting in motion the right procedure to satisfy the
	requesting thread.
	It thus creates the abstraction, through SSIRequest, of the existence of
	System Calls, despite the underlying message passing structure.

	The SSI also manages communication between Device and threads,
	appropriately storing with arrays and structures the useful
	information depending on the situations.

	Finally, it manages, together with the exception and scheduler routines,
	the virtual device of the Pseudo Clock Tick.

**********************************************************************/
void SSI_thread(){

	volatile struct SSI_request_msg *req_struct;
	tcb_t *req_thread;
	tcb_t *new_thread;
	tcb_t *parent;
	tcb_t *wfio_thread;
	int i;
	int device_num;

	/* Initialize arrays for Waitforio and Interrupt */
	for (i=0;i<48;i++) {
		devStatus_array[i] = -1;
		devTcb_array[i] = NULL;
	}

	/* Initialize array for Waitforclock */
	for (i=0;i<MAXTHREADS;i++) {
		clockTcb_array[i] = NULL;
	}

	/* Infinite loop */
	while (TRUE) {

		/* Fundamental step: wait for service requests */
		req_thread = MsgRecv(RECV,ANYMESSAGE,&req_struct);

		/* Check if the request is Interrupt/Waitforclock */
		switch (req_struct->service) {

				case (PSEUDOCLOCK_MSG):
					/* Wake up all threads waiting for Pseudo Clock Tick and clean the array */
					for (i=0;i<(MAXTHREADS-1);i++) {
						if (clockTcb_array[i] != NULL) {
							/* This will be enough to unblock the thread */
							MsgSend(SEND, clockTcb_array[i], 42);
							clockTcb_array[i] = 0;
						}
						else break;
					}

					break;

				case (INTERRUPT_MSG):
					/* 2 cases - Sender is in this case the Device Register address */
					device_num = rest_index((memaddr)req_thread);

					/* Corresponding WAITFORIO not yet arrived */
					if ((wfio_thread = devTcb_array[device_num]) == NULL)
						/* Store at the corresponding position of the array the status to be retrieved */
						devStatus_array[device_num] = req_struct->arg;

					/* Corresponding WAITFORIO already arrived */
					else {
						MsgSend(SEND, wfio_thread, ((req_struct->arg) & STATUSMASK));
						devTcb_array[device_num] = NULL;
					}

					break;

				/* It's not an interrupt */
				default:
					break;
		}


		/* First verify that the requesting thread still exists */
		if (((thereIsThread(&ready_queue, req_thread)) != NULL ) ||
				((thereIsThread(&wait_queue, req_thread)) != NULL )) {

			/* Check what type of request arrived */
			switch (req_struct->service) {

				case (WAITFORCLOCK):
						/*
							Keep stored the tcbs stopped waiting for Pseudo Clock Tick
							that are already in wait_queue due to the blocking MsgRecv.
						*/
						for (i=0;i<(MAXTHREADS-1);i++) {
							if (clockTcb_array[i] == NULL) {
								clockTcb_array[i] = req_thread;
								break;
							}
						}

						break;


				case (CREATEBROTHER):
						/*
							Creates a brother of the requesting thread
							adding it to the process tree.
							The state of the process to create is passed
							by pointer as payload.
					 */
						if ((new_thread = allocTcb()) == NULL ) {
							MsgSend(SEND, req_thread, CREATENOGOOD);
						}
						/* If it has no parent, just insert the thread among siblings */
						if ((parent = req_thread->t_parent) == NULL) {
							insertSibling(req_thread, new_thread);
						}
						else insertChild(parent, new_thread);
						thread_count++;
						/* Set the state and load in ready queue */
						save_state((state_t *)req_struct->arg, &(new_thread->t_state));

						insertThread(&ready_queue, new_thread);

						/* Return the pointer to the new thread */
						MsgSend(SEND, req_thread, new_thread);

						break;


				case (CREATESON):

						/*
							Creates a child of the requesting thread
							adding it to the process tree.
							The state of the process to create is passed
							by pointer as payload.
					 */
						if ((new_thread = allocTcb()) == NULL ) {
							MsgSend(SEND, req_thread, CREATENOGOOD);
						}
						insertChild(req_thread, new_thread);
						thread_count++;
						/* Set the state */
						save_state((state_t *)req_struct->arg, &(new_thread->t_state));

						/* Inherits Trap Manager if defined, otherwise remain NULL */
						new_thread->sysbp_manager_thread = req_thread->sysbp_manager_thread;
						new_thread->pgmtrap_manager_thread = req_thread->pgmtrap_manager_thread;
						new_thread->tlbtrap_manager_thread = req_thread->tlbtrap_manager_thread;

						/* Load in ready queue */
						insertThread(&ready_queue, new_thread);

						/* Return the pointer to the new thread */
						MsgSend(SEND, req_thread, new_thread);

						break;


				case (TERMINATE):
						/* Terminate the thread together with the entire subtree (see terminate) */
						terminate(req_thread);
						break;


				case (SPECPRGMGR):
						/*
							If a manager has already been specified for this exception or the
							indicated one doesn't exist, the thread and subtree are terminated.
						*/
						if ( (req_thread->pgmtrap_manager_thread != NULL) ||
							 	((!(thereIsThread(&ready_queue, ((tcb_t *)req_struct->arg)))) &&
								(!(thereIsThread(&wait_queue, ((tcb_t *)req_struct->arg))))) )
								terminate(req_thread);
						/* Otherwise the manager is set in the thread structure */
						else {
							/* Add the manager to the trap_managers array */
							add_manager((tcb_t *)req_struct->arg);
							/* Set the requester's field specifying the manager */
							req_thread->pgmtrap_manager_thread = (tcb_t *)req_struct->arg;

							MsgSend(SEND, req_thread, 0);
						}

						break;


				case (SPECTLBMGR):
						/*
							If a manager has already been specified for this exception or the
							indicated one doesn't exist, the thread and subtree are terminated.
						*/
						if ( (req_thread->tlbtrap_manager_thread != NULL) ||
							 	((!(thereIsThread(&ready_queue, ((tcb_t *)req_struct->arg)))) &&
								(!(thereIsThread(&wait_queue, ((tcb_t *)req_struct->arg))))) )
								terminate(req_thread);
						/* Otherwise the manager is set in the thread structure */
						else {
							/* Add the manager to the trap_managers array */
							add_manager((tcb_t *)req_struct->arg);
							/* Set the requester's field specifying the manager */
							req_thread->tlbtrap_manager_thread = (tcb_t *)req_struct->arg;

							MsgSend(SEND, req_thread, 0);
						}

						break;


				case (SPECSYSMGR):
						/*
							If a manager has already been specified for this exception or the
							indicated one doesn't exist, the thread and subtree are terminated.
						*/
						if ( (req_thread->sysbp_manager_thread != NULL) ||
							 	((!(thereIsThread(&ready_queue, ((tcb_t *)req_struct->arg)))) &&
								(!(thereIsThread(&wait_queue, ((tcb_t *)req_struct->arg))))) )
								terminate(req_thread);
						/* Otherwise the manager is set in the thread structure */
						else {
							/* Add the manager to the trap_managers array */
							add_manager((tcb_t *)req_struct->arg);
							/* Set the requester's field specifying the manager */
							req_thread->sysbp_manager_thread = (tcb_t *)req_struct->arg;

							MsgSend(SEND, req_thread, 0);
						}

						break;


				case (GETCPUTIME):
							/* Returns to the thread the total time spent in the CPU */
							MsgSend(SEND, req_thread, req_thread->cpu_time);
						break;


				case (WAITFORIO):
						/* 2 cases */
						if ((device_num = rest_index(req_struct->arg)) == -1) terminate(req_thread);

						/* Interrupt from corresponding device not yet arrived */
						else {
							if (devStatus_array[device_num] == -1)
								/* Store at the corresponding position of the array the *tcb to be unblocked */
								devTcb_array[device_num] = req_thread;

							/* Interrupt from corresponding device already arrived */
							else {
								MsgSend(SEND, req_thread, devStatus_array[device_num]);
								devStatus_array[device_num] = -1;
							}
						}
						break;


				default:
					PANIC(); /* Error in SSIRequest */

			}

		}
	}
}


