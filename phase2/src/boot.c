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

	This module implements the system boot by setting global variables
	and part of queues, lists, and structures useful for the kernel and
	SSI, also populates the Four New Areas for exceptions.

************************************************************************/

/* Phase1 */
#include <msg.e>
#include <tcb.e>
/* Phase2 */
#include <scheduler.e>
#include <interrupts.e>
#include <exceptions.e>
#include <ssi.e>


/* External declaration of the testing function and SSI */
extern void test (void);
extern void SSI_thread(void);


/**********************************************************************
														GLOBAL VARIABLES
**********************************************************************/

/* Queue of processes ready to be executed on the CPU */
struct list_head ready_queue;

/* Queue of waiting processes (I/O, WAITFORCLOCK, Freeze...) */
struct list_head wait_queue;

/* Thread counter in the system */
U32 thread_count;

/* Counter of threads waiting for I/O or SSI service */
U32 soft_block_count;

/* Pointer to the currently active Thread Control Block in the system */
tcb_t *current_thread;

/* Previous TOD value for calculating thread times */
U32 current_thread_tod;

/* Pseudo Tick elapsed so far */
U32 pseudo_tick;

/* Previous TOD value for calculating the Pseudo Clock Tick */
U32 start_pseudo_tick;

/* Pointer to Thread control block for SSI */
tcb_t *SSI_tcb;

/* Pointer to Thread control block for TEST */
tcb_t *TEST_tcb;



/**********************************************************************
														AREA_POPULATE

	Populates the Four New Areas, where the states of the exception
	handlers that will be loaded when an exception is raised will be.

**********************************************************************/

HIDDEN void area_populate(memaddr area, memaddr handler) {

	/* CPU state for the new area */
	state_t *new_area_state;

	/* Point to the new area being processed */
	new_area_state = (state_t *) area;

	/* Save the current CPU state into the new area and then modify what's needed */
	STST(new_area_state);

	/* Set PC (and t9 register) to the handler's address */
	new_area_state->pc_epc = new_area_state->reg_t9 = handler;

	/* Set SP to RAMTOP -> Each handler will use the last RAM frame for its stack */
	new_area_state->reg_sp = RAMTOP;

	/* Set Kernel Mode ON */
	new_area_state->status &= ~STATUS_KUc;

	/* Set No virtual memory */
	new_area_state->status &= ~STATUS_VMc;

	/* Set No interrupt enabled */
	new_area_state->status &= ~STATUS_IEc;
}



/**********************************************************************
														MAIN

	Populates the Four New Areas, where the states of the exception
	handlers that will be loaded when an exception is raised will be.

**********************************************************************/
int main () {

	/* Populate the four New Areas */

	/* SYSCALL/Breakpoint */
	area_populate(SYSBK_NEWAREA,(memaddr)sysbp_handler);

	/* Program Traps */
	area_populate(PGMTRAP_NEWAREA,(memaddr)pgmtrap_handler);

	/* TLB management */
	area_populate(TLB_NEWAREA,(memaddr)tlb_handler);

	/* Interrupts */
	area_populate(INT_NEWAREA,(memaddr)int_handler);

	/* Initialize thread and message lists that will be used in the system */
	initTcbs();
	initMsg();

	/* Create the two main queues of the system */
	mkEmptyThreadQ(&ready_queue);
	mkEmptyThreadQ(&wait_queue);

	/* Initialize global variables */
	thread_count = 0;
	soft_block_count = 0;
	current_thread = NULL;
	pseudo_tick = 0;
	start_pseudo_tick = 0;
	current_thread_tod = 0;

	/* Allocate thread for SSI and set its TCB fields */
	if ((SSI_tcb = allocTcb()) == NULL)	PANIC();

	/* YES interrupt (All), NO Virtual Memory, Kernel Mode ON */
	SSI_tcb->t_state.status = (SSI_tcb->t_state.status | STATUS_IEp | STATUS_INT_UNMASKED) & ~STATUS_KUp & ~STATUS_VMp;

	/* Assign a stack to the thread */
	SSI_tcb->t_state.reg_sp = RAMTOP - FRAME_SIZE;

	/* PC initialized to SSI thread's start address and for technical reasons also to reg_t9 */
	SSI_tcb->t_state.pc_epc = SSI_tcb->t_state.reg_t9 = (memaddr)SSI_thread;

	/* Load SSI into the ready queue so it will be the first thread called by the scheduler */
	insertThread(&ready_queue, SSI_tcb);

	thread_count++;


	/* Allocate thread for TEST */
	if ((TEST_tcb=allocTcb()) == NULL )	PANIC();

	/* YES interrupt (All), NO Virtual Memory, Kernel Mode ON */
	TEST_tcb->t_state.status = (TEST_tcb->t_state.status | STATUS_IEp | STATUS_INT_UNMASKED) & ~STATUS_KUp & ~STATUS_VMp;

	/* Assign a stack to the thread */
	TEST_tcb->t_state.reg_sp = RAMTOP - (2*FRAME_SIZE);

	/* PC initialized to TEST thread's start address and for technical reasons also to reg_t9 */
	TEST_tcb->t_state.pc_epc = TEST_tcb->t_state.reg_t9 = (memaddr)test;

	/* Load TEST into the ready queue so it will be the second thread called by the scheduler */
	insertThread(&ready_queue, TEST_tcb);

	thread_count++;


	/* Get the initial time for Pseudo Clock Tick counting and start the scheduler */
	start_pseudo_tick = GET_TODLOW;

	scheduler();

	return 0;

}
