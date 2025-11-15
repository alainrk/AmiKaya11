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

	This module implements a scheduler with round robin policy and
	provides part of the services useful for time management.

************************************************************************/


/* Phase1 */
#include <msg.e>
#include <tcb.e>
/* Phase2 */
#include <boot.e>


/**********************************************************************
														 SCHEDULER

	Complementary to the exception and interrupt handler for time
	management, it handles:

	-Detecting shutdown situation and invoking the HALT() routine
	-Detecting deadlock situation and invoking the PANIC() routine
	-Detecting wait situation, waiting for an interrupt
	-Loading processes on the CPU with a Round-Robin policy
	-Setting the Interval Timer ensuring both time slice fairness for
		threads and the abstraction of the Pseudo Clock Tick virtual device

**********************************************************************/
void scheduler () {

	/* No process currently loaded on the CPU */
	if (current_thread == NULL) {

		/* Ready Queue empty: distinction of 3 special cases */
		if (emptyThreadQ(&ready_queue)) {

			/* The only process on the CPU is SSI -> Normal System Shutdown */
			if (thread_count == 1) HALT();

			/*
				Simple Deadlock Detection:
					All processes in the system are stopped waiting for
					a message but not for services or I/O, thus a situation
					not solvable with input from SSI or Interrupt.
					The PANIC() routine is invoked.
			*/
			if ((thread_count > 0) && (soft_block_count == 0)) PANIC();

			/* Wait state, the processes in the system are waiting for SSI service or I/O */
			if ((thread_count > 0) && (soft_block_count > 0)) {
				/* Set the processor state enabling Interrupts and wait */
				setSTATUS((getSTATUS() | STATUS_IEc | STATUS_INT_UNMASKED));
				/* With new istruction of umps2 */
				WAIT();
				/* Without WAIT() istruction
				 * while(TRUE) ;
				 */
			}

			else PANIC();
		}

		/* Ready Queue not empty */
		else {
			/* Remove the first thread from ready queue and set it as current thread */
			if ((current_thread = removeThread(&ready_queue)) == NULL) PANIC();

			/* Update the Pseudo Clock Tick */
			pseudo_tick += (GET_TODLOW - start_pseudo_tick);
			/* Immediately get the time again */
			start_pseudo_tick = GET_TODLOW;

			/* Thread time management: initialize time slice in the appropriate TCB field */
			current_thread->cpu_slice = 0;
			/* Get the initial time useful for the next update */
			current_thread_tod = GET_TODLOW;

			/* If the pseudo clock has been exceeded, trigger the interrupt immediately */
			if (pseudo_tick >= SCHED_PSEUDO_CLOCK)
				SET_IT(1);

			/* To manage Time Slice and Pseudo Clock Tick, set the minimum time between the two deadlines */
			else
				SET_IT(MIN(SCHED_TIME_SLICE, (SCHED_PSEUDO_CLOCK - pseudo_tick)));

			/* Load the processor state and thus start the thread */
			LDST(&(current_thread->t_state));

			PANIC();
		}
	}

	/* A thread is already active in the system */
	else if (current_thread != NULL) {

		/* As above: calculate timings for Pseudo Clock and Thread Time Slice */
		pseudo_tick += (GET_TODLOW - start_pseudo_tick);
		start_pseudo_tick = GET_TODLOW;

		current_thread_tod = GET_TODLOW;

		/* As above: Interval Timer setting */
		if (pseudo_tick >= SCHED_PSEUDO_CLOCK)
			SET_IT(1);

		else
			SET_IT(MIN((SCHED_TIME_SLICE - current_thread->cpu_slice),(SCHED_PSEUDO_CLOCK - pseudo_tick)));

		LDST(&(current_thread->t_state));
	}

}
