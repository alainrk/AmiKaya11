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

	Questo modulo implementa il Sistem Service Interface, che fornisce una 
	vera e propria interfaccia verso l'esterno del nucleo, e che nel 
	contempo dialoga con esso per la gestione di Pseudo Clock Tick e 
	interrupt.

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

	Assegna all'indirizzo di un Device Register un indice utilizzato
	negli array usati dall'SSI per la gestione di interrupt/WaitforIO.

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
	Array dove si memorizzerà un campo stato del corrispondente 
	Device Register qualora non sia ancora arrivato un WAITFORIO
	corrispondente.
*/
HIDDEN int devStatus_array[48];

/* 
	Array dove si memorizzerà un puntatore a TCB per il thread  che ha 
	già fatto WAITFORIO un Device dal quale Device Register non è ancora
	arrivato un interrupt.
*/
HIDDEN tcb_t *devTcb_array[48];

/* 
	Array per la gestione dello Pseudo Clock.
	Vi verranno memorizzati i puntatori a TCB di quei thread che hanno	
	richiesto il servizio WAITFORCLOCK.
*/
HIDDEN tcb_t *clockTcb_array[MAXTHREADS];


/*
	Struttura utile per inviare messaggio di richiesta all'SSI
	in quanto mappa nell'unico campo "U32 payload" di MsgSend
	i 3 campi service, payload, reply di SSIRequest.

	L'SSI dovrà quindi trattare un messaggio nel cui interno
	ci sarà una struttura fatta in questo modo, e usare le 
	informazioni in essa contenuta per fornire il servizio.
*/
struct SSI_request_msg {
	U32 service;		/* Servizio richiesto */
	U32 arg;				/* Parametro eventuale */
	U32 *reply; 		/* Dove andrà messa la risposta */
};



/**********************************************************************
										      	SSIREQUEST

	SSIRequest implementata come combinazione di:
	MsgSend ---> Invia tipo di richiesta a SSI come messaggio.
	MsgRecv ---> Aspetta (bloccante) la risposta da SSI.

	Fondamentale l'utilizzo delle strutture SSI_request_msg.

**********************************************************************/
void SSIRequest (U32 service, U32 payload, U32 *reply) {

	/* Creo struttura per invio del messaggio */
	struct SSI_request_msg request;

	/* Mappatura dei campi */
	request.service = service;
	request.arg = payload; /* Potrebbe essere 0 se non ci sono */
	request.reply = reply;

	/* Invio messaggio con indirizzo della struttura appena creata */
	MsgSend(SEND,MAGIC_SSI,&request);

	/* Mi metto in attesa come un normale messaggio */
	MsgRecv(RECV,MAGIC_SSI,reply);

}



/**********************************************************************
										      	SSI_THREAD

	Il Sistem Service Interface è un thread che lavora in modo simile 
	ad un server RPC, aspettando l'arrivo di una richiesta e agendo di 
	conseguenza, mettendo in moto la giusta procedura per soddisfare il
	thread richiedente.
	Crea quindi l'astrazione, mediante SSIRequest dell'esistenza di 
	System Call, nonostante la sottostruttura di message passing.

	L'SSI inoltre gestisce la comunicazione tra Device e thread, 
	memorizzando opportunamente con array e strutture le informazioni
	utili a seconda delle situazioni.

	Infine gestisce, assieme alle routine per le eccezioni e lo scheduler,
	il device virtuale dello Pseudo Clock Tick.

**********************************************************************/
void SSI_thread(){

	volatile struct SSI_request_msg *req_struct;
	tcb_t *req_thread;
	tcb_t *new_thread;
	tcb_t *parent;
	tcb_t *wfio_thread;
	int i;
	int device_num;

	/* Inizializzo gli array per Waitforio e Interrupt */
	for (i=0;i<48;i++) {
		devStatus_array[i] = -1;
		devTcb_array[i] = NULL;
	}

	/* Inizializzo array per Waitforclock */
	for (i=0;i<MAXTHREADS;i++) {
		clockTcb_array[i] = NULL;
	}
	
	/* Ciclo infinito */
	while (TRUE) {

		/* Passo fondamentale: mi metto in attesa di richieste di servizi */
		req_thread = MsgRecv(RECV,ANYMESSAGE,&req_struct);

		/* Verifico se la richiesta è Interrupt/Waitforclock */
		switch (req_struct->service) {
		
				case (PSEUDOCLOCK_MSG):
					/* Sveglio tutti i thread in attesa dello Pseudo Clock Tick e pulisco l'array */
					for (i=0;i<(MAXTHREADS-1);i++) {
						if (clockTcb_array[i] != NULL) {
							/* Questo basterà a sbloccare il thread */
							MsgSend(SEND, clockTcb_array[i], 42);
							clockTcb_array[i] = 0;
						}
						else break;
					}

					break;

				case (INTERRUPT_MSG):
					/* 2 casi - Sender è in questo caso l'indirizzo del Device Register */
					device_num = rest_index((memaddr)req_thread);

					/* WAITFORIO corrispondente non ancora arrivato */
					if ((wfio_thread = devTcb_array[device_num]) == NULL)
						/* Memorizzo alla posizione corrispondente dell'array lo status che dovrà essere prelevato */
						devStatus_array[device_num] = req_struct->arg;

					/* WAITFORIO corrispondente già arrivato */
					else {
						MsgSend(SEND, wfio_thread, ((req_struct->arg) & STATUSMASK));
						devTcb_array[device_num] = NULL;
					}
	
					break;
				
				/* Non è un interrupt */
				default:
					break;
		}


		/* Verifico innanzitutto che il thread richiedente esista ancora */
		if (((thereIsThread(&ready_queue, req_thread)) != NULL ) || 
				((thereIsThread(&wait_queue, req_thread)) != NULL )) {

			/* Verifico che tipo di richiesta è arrivata */
			switch (req_struct->service) {

				case (WAITFORCLOCK):
						/* 
							Tengo memorizzati i tcb fermi ad aspettare lo Pseudo Clock Tick 
							che sono già in wait_queue per effetto della MsgRecv bloccante. 
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
							Crea un fratello del thread richiedente
							aggiungendolo all'albero dei processi.
							Lo stato del processo da creare è passato
							per puntatore come payload.
					 */
						if ((new_thread = allocTcb()) == NULL ) {
							MsgSend(SEND, req_thread, CREATENOGOOD);
						}
						/* Se non ha un padre basta inserisco il thread tra i fratelli */
						if ((parent = req_thread->t_parent) == NULL) {
							insertSibling(req_thread, new_thread);
						}
						else insertChild(parent, new_thread);
						thread_count++;
						/* Setto lo stato e carico in ready queue*/
						save_state((state_t *)req_struct->arg, &(new_thread->t_state));

						insertThread(&ready_queue, new_thread);

						/* Restituisco il puntatore al nuovo thread */
						MsgSend(SEND, req_thread, new_thread);

						break;


				case (CREATESON):

						/* 
							Crea un figlio del thread richiedente
							aggiungendolo all'albero dei processi.
							Lo stato del processo da creare è passato
							per puntatore come payload.
					 */
						if ((new_thread = allocTcb()) == NULL ) {
							MsgSend(SEND, req_thread, CREATENOGOOD);
						}
						insertChild(req_thread, new_thread);
						thread_count++;
						/* Setto lo stato */
						save_state((state_t *)req_struct->arg, &(new_thread->t_state));

						/* Eredita Trap Manager se definiti, altrimenti restano NULL */
						new_thread->sysbp_manager_thread = req_thread->sysbp_manager_thread;
						new_thread->pgmtrap_manager_thread = req_thread->pgmtrap_manager_thread;
						new_thread->tlbtrap_manager_thread = req_thread->tlbtrap_manager_thread;

						/* Carico in ready queue */
						insertThread(&ready_queue, new_thread);

						/* Restituisco il puntatore al nuovo thread */
						MsgSend(SEND, req_thread, new_thread);

						break;

			
				case (TERMINATE):
						/* Termina il thread assieme a tutto il sottoalbero (vedi terminate) */
						terminate(req_thread);
						break;


				case (SPECPRGMGR):
						/* 
							Se è già stato specificato un manager per questa eccezione o non 
							esiste quello indicato, il thread e il sottoalbero viene terminato. 
						*/
						if ( (req_thread->pgmtrap_manager_thread != NULL) ||
							 	((!(thereIsThread(&ready_queue, ((tcb_t *)req_struct->arg)))) &&
								(!(thereIsThread(&wait_queue, ((tcb_t *)req_struct->arg))))) )
								terminate(req_thread);
						/* Altrimenti viene settato nella struttura del thread il manager */
						else {
							/* Aggiungo il manager al trap_managers array */
							add_manager((tcb_t *)req_struct->arg);
							/* Setto il campo del richiedente specificando il manager */
							req_thread->pgmtrap_manager_thread = (tcb_t *)req_struct->arg;

							MsgSend(SEND, req_thread, 0);
						}

						break;


				case (SPECTLBMGR):
						/* 
							Se è già stato specificato un manager per questa eccezione o non 
							esiste quello indicato il thread e il sottoalbero viene terminato. 
						*/
						if ( (req_thread->tlbtrap_manager_thread != NULL) ||
							 	((!(thereIsThread(&ready_queue, ((tcb_t *)req_struct->arg)))) &&
								(!(thereIsThread(&wait_queue, ((tcb_t *)req_struct->arg))))) )
								terminate(req_thread);
						/* Altrimenti viene settato nella struttura del thread il manager */
						else {
							/* Aggiungo il manager al trap_managers array */
							add_manager((tcb_t *)req_struct->arg);
							/* Setto il campo del richiedente specificando il manager */
							req_thread->tlbtrap_manager_thread = (tcb_t *)req_struct->arg;

							MsgSend(SEND, req_thread, 0);
						}

						break;


				case (SPECSYSMGR):
						/* 
							Se è già stato specificato un manager per questa eccezione o non 
							esiste quello indicato il thread e il sottoalbero viene terminato. 
						*/
						if ( (req_thread->sysbp_manager_thread != NULL) ||
							 	((!(thereIsThread(&ready_queue, ((tcb_t *)req_struct->arg)))) &&
								(!(thereIsThread(&wait_queue, ((tcb_t *)req_struct->arg))))) )
								terminate(req_thread);
						/* Altrimenti viene settato nella struttura del thread il manager */
						else {
							/* Aggiungo il manager al trap_managers array */
							add_manager((tcb_t *)req_struct->arg);
							/* Setto il campo del richiedente specificando il manager */
							req_thread->sysbp_manager_thread = (tcb_t *)req_struct->arg;

							MsgSend(SEND, req_thread, 0);
						}

						break;


				case (GETCPUTIME):
							/* Restituisce al thread il tempo totale passato nella CPU */
							MsgSend(SEND, req_thread, req_thread->cpu_time);
						break;


				case (WAITFORIO):
						/* 2 casi */
						if ((device_num = rest_index(req_struct->arg)) == -1) terminate(req_thread);

						/* Interrupt da device corrispondente non ancora arrivato */
						else {
							if (devStatus_array[device_num] == -1)
								/* Memorizzo alla posizione corrispondente dell'array il *tcb che dovrà essere sbloccato */
								devTcb_array[device_num] = req_thread;

							/* Interrupt da device corrispondente già arrivato */
							else {
								MsgSend(SEND, req_thread, devStatus_array[device_num]);
								devStatus_array[device_num] = -1;
							}
						}
						break;


				default:
					PANIC(); /* Errore in SSIRequest */

			}

		}
	}
}


