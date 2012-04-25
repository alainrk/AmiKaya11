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

/*************************** SCHEDULER.C *******************************

	Questo modulo implementa uno scheduler con politica round robin e 
	fornisce una parte dei servizi utili per la gestione del tempo.

************************************************************************/


/* Phase1 */
#include <msg.e>
#include <tcb.e>
/* Phase2 */
#include <boot.e>


/**********************************************************************
														 SCHEDULER

	Complementare del gestore delle eccezioni e degli interrupt per la 
	gestione del tempo,si occupa di:

	-Rilevare la situazione di shutdown e invocare la routine HALT()
	-Rilevare la situazione di deadlock e invocare la routine PANIC()
	-Rilevare la situazione di wait, aspettando un interrupt
	-Caricare i processi sulla CPU con una politica Round-Robin
	-Settare l'Interval Timer garantendo sia l'equità di time slice per i 
		thread, sia l'astrazione di device virtuale Pseudo Clock Tick

**********************************************************************/
void scheduler () {
	
	/* Nessun processo attualmente caricato sulla CPU */
	if (current_thread == NULL) {
		
		/* Ready Queue vuota: distinzione dei 3 casi particolari */
		if (emptyThreadQ(&ready_queue)) {

			/* L'unico processo sulla CPU è l'SSI -> Normal System Shutdown */
			if (thread_count == 1) HALT();

			/* 
				Semplice Deadlock Detection:
					Tutti i processi nel sistema sono fermi in attesa di
					un messaggio ma non di servizi o I/O, quindi situazione
					non risolvibile con input da SSI o Interrupt.
					Viene invocata la routine PANIC().
			*/
			if ((thread_count > 0) && (soft_block_count == 0)) PANIC();

			/* Wait state, i processi che sono nel sistema aspettano servizio SSI o I/O */
			if ((thread_count > 0) && (soft_block_count > 0)) {
				/* Setto lo stato del processore abilitando gli Interrupt e aspetto */
				setSTATUS((getSTATUS() | STATUS_IEc | STATUS_INT_UNMASKED));
				/* With new istruction of umps2 */				
				WAIT();
				/* Without WAIT() istruction 
				 * while(TRUE) ;
				 */
			}

			else PANIC();
		}

		/* Ready Queue non vuota */
		else {
			/* Tolgo il primo thread dalla ready queue e lo imposto come thread corrente */
			if ((current_thread = removeThread(&ready_queue)) == NULL) PANIC();
		
			/* Aggiorno lo Pseudo Clock Tick */
			pseudo_tick += (GET_TODLOW - start_pseudo_tick);
			/* Riprendo subito il tempo */
			start_pseudo_tick = GET_TODLOW;

			/* Gestione tempi thread: inizializzo time slice nel campo apposito del TCB */
			current_thread->cpu_slice = 0;
			/* Prendo il tempo inizale utile all'aggiornamento successivo */
			current_thread_tod = GET_TODLOW;

			/* Se lo pseudo clock è stato superato faccio scattare subito l'interrupt */
			if (pseudo_tick >= SCHED_PSEUDO_CLOCK)
				SET_IT(1);

			/* Per tenere la gestione di Time Slice e Pseudo Clock Tick setto il tempo minimo fra le due scadenze */
			else 
				SET_IT(MIN(SCHED_TIME_SLICE, (SCHED_PSEUDO_CLOCK - pseudo_tick)));

			/* Carico lo stato del processore e quindi parte il thread */
			LDST(&(current_thread->t_state));

			PANIC();
		}
	}

	/* Un thread è già attivo nel sistema */
	else if (current_thread != NULL) {
	
		/* Come sopra: calcolo delle tempistiche per Pseudo Clock e Thread Time Slice */
		pseudo_tick += (GET_TODLOW - start_pseudo_tick);
		start_pseudo_tick = GET_TODLOW;

		current_thread_tod = GET_TODLOW;

		/* Come sopra: settaggio Interval Timer */
		if (pseudo_tick >= SCHED_PSEUDO_CLOCK)
			SET_IT(1);

		else 
			SET_IT(MIN((SCHED_TIME_SLICE - current_thread->cpu_slice),(SCHED_PSEUDO_CLOCK - pseudo_tick)));

		LDST(&(current_thread->t_state));
	}

}
