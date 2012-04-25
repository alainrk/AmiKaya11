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

/********************************* BOOT.C *******************************

	Questo modulo implementa il boot del sistema impostando le variabili
	globali e una parte di code, liste, e strutture utili al nucleo e 
	all'SSI, inoltre popola le Four New Areas delle exceptions.

************************************************************************/

/* Phase1 */
#include <msg.e>
#include <tcb.e>
/* Phase2 */
#include <scheduler.e>
#include <interrupts.e>
#include <exceptions.e>
#include <ssi.e>


/* Dichiarazione esterna della funzione di testing e dell'SSI */
extern void test (void);
extern void SSI_thread(void);


/**********************************************************************
														VARIABILI GLOBALI
**********************************************************************/

/* Coda processi pronti ad essere eseguiti sulla CPU */
struct list_head ready_queue;

/* Coda processi in attesa (I/O, WAITFORCLOCK, Freeze...) */
struct list_head wait_queue;

/* Contatore thread nel sistema */
U32 thread_count;

/* Contatore thread in attesa di I/O o servizio SSI */
U32 soft_block_count;

/* Puntatore al Thread Control Block attualmente attivo nel sistema */
tcb_t *current_thread;

/* Valore precedente del TOD per il calcolo dei tempi del thread */
U32 current_thread_tod;

/* Pseudo Tick passato finora */
U32 pseudo_tick;

/* Valore precedente del TOD per il calcolo dello Pseudo Clock TIck */
U32 start_pseudo_tick;

/* Puntantore a Thread control block per SSI */
tcb_t *SSI_tcb;

/* Puntantore a Thread control block per TEST */
tcb_t *TEST_tcb;



/**********************************************************************
														AREA_POPULATE

	Popola le Four New Areas, dove saranno cioè gli stati degli exception
	handler che verranno caricati con il sollevamento di un'eccezione.

**********************************************************************/

HIDDEN void area_populate(memaddr area, memaddr handler) {

	/* CPU state per la new area */
	state_t *new_area_state;

	/*Vado a puntare la new area che sto trattando */
	new_area_state = (state_t *) area;
	
	/*Salvataggio dentro alla new area dello stato corrente CPU e poi modifico quello che mi interessa*/
	STST(new_area_state);

	/* Set PC (e registro t9) all'indirizzo dell'handler */
	new_area_state->pc_epc = new_area_state->reg_t9 = handler;
	
	/* Set SP a RAMTOP -> Ogni handler userà l'ultimo frame della RAM per il suo stack */
	new_area_state->reg_sp = RAMTOP;
	
	/* Set Kernel Mode ON */
	new_area_state->status &= ~STATUS_KUc;

	/* Set No virtual memory */
	new_area_state->status &= ~STATUS_VMc;

	/* Set No interrupt enabled */
	new_area_state->status &= ~STATUS_IEc;
}



/**********************************************************************
														AREA_POPULATE

	Popola le Four New Areas, dove saranno cioè gli stati degli exception
	handler che verranno caricati con il sollevamento di un'eccezione.

**********************************************************************/
int main () {

	/* Popolo le four New Areas */

	/* SYSCALL/Breakpoint */
	area_populate(SYSBK_NEWAREA,(memaddr)sysbp_handler);

	/* Program Traps */
	area_populate(PGMTRAP_NEWAREA,(memaddr)pgmtrap_handler);

	/* TLB management */
	area_populate(TLB_NEWAREA,(memaddr)tlb_handler);

	/* Interrupts */
	area_populate(INT_NEWAREA,(memaddr)int_handler);

	/* Inizializzo liste di thread e messaggi che saranno usate nel sistema */
	initTcbs();
	initMsg();

	/* Creo le due code principali del sistema */
	mkEmptyThreadQ(&ready_queue);
	mkEmptyThreadQ(&wait_queue);

	/* Inizializzo le variabili globali */
	thread_count = 0;
	soft_block_count = 0;
	current_thread = NULL;
	pseudo_tick = 0;
	start_pseudo_tick = 0;
	current_thread_tod = 0;

	/* Alloco thread per l'SSI e setto i campi del suo TCB */
	if ((SSI_tcb = allocTcb()) == NULL)	PANIC();

	/* SI interrupt (Tutti), NO Virtual Memory, Kernel Mode ON */
	SSI_tcb->t_state.status = (SSI_tcb->t_state.status | STATUS_IEp | STATUS_INT_UNMASKED) & ~STATUS_KUp & ~STATUS_VMp;

	/* Assegno uno stack al thread */
	SSI_tcb->t_state.reg_sp = RAMTOP - FRAME_SIZE;

	/* PC inizializzato a indirizzo di inizio del thread dell'SSI e per ragioni tecniche parallelamente anche a reg_t9 */
	SSI_tcb->t_state.pc_epc = SSI_tcb->t_state.reg_t9 = (memaddr)SSI_thread;

	/* Carico SSI nella ready queue e sarà quindi il primo thread chiamato dallo scheduler */
	insertThread(&ready_queue, SSI_tcb);

	thread_count++;


	/* Alloco thread per TEST */
	if ((TEST_tcb=allocTcb()) == NULL )	PANIC();

	/* SI interrupt (Tutti), NO Virtual Memory, Kernel Mode ON */
	TEST_tcb->t_state.status = (TEST_tcb->t_state.status | STATUS_IEp | STATUS_INT_UNMASKED) & ~STATUS_KUp & ~STATUS_VMp;

	/* Assegno uno stack al thread */
	TEST_tcb->t_state.reg_sp = RAMTOP - (2*FRAME_SIZE);

	/* PC inizializzato a indirizzo di inizio del thread TEST e per ragioni tecniche parallelamente anche a reg_t9 */
	TEST_tcb->t_state.pc_epc = TEST_tcb->t_state.reg_t9 = (memaddr)test;

	/* Carico TEST nella ready queue e sarà quindi il secondo thread chiamato dallo scheduler */
	insertThread(&ready_queue, TEST_tcb);

	thread_count++;


	/* Prendo il tempo iniziale per il conteggio dello Pseudo Clock Tick e faccio partire lo scheduler */
	start_pseudo_tick = GET_TODLOW;

	scheduler();

	return 0;

}
