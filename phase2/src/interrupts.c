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

/*************************** INTERRUPTS.C *******************************

	This module implements the interrupt handling routine, and the
	communication for this purpose with the SSI.

************************************************************************/


/* Phase1 */
#include <msg.e>
#include <tcb.e>
/* Phase2 */
#include <boot.e>
#include <scheduler.e>
#include <exceptions.e>
#include <ssi.e>


/* Macro for readability: reference to Old Area as processor state */
#define	int_oldarea	((state_t *)INT_OLDAREA)

/* Magic numbers for Interrupt to SSI */
#define INTERRUPT_MSG			255
#define PSEUDOCLOCK_MSG		254
#define NOREPLY						0


/* Global structure array for interrupt messages to SSI */
struct SSI_request_msg interrupt_msg_array[48];
/* Used to scroll interrupt_msg_array[] in circular mode */
HIDDEN int i=0;


/**********************************************************************
														 WHICH_DEVICE

	Identifies the device index taking as parameter the bitmap
	relative to the interrupt line being considered.
	Satisfies priority by scanning sequentially from the lower index
	to the higher one.

**********************************************************************/
HIDDEN int which_device (int bitmap) {
	if (bitmap == (bitmap | 0x1)) return 0;
	if (bitmap == (bitmap | 0x2)) return 1;
	if (bitmap == (bitmap | 0x4)) return 2;
	if (bitmap == (bitmap | 0x8)) return 3;
	if (bitmap == (bitmap | 0x10)) return 4;
	if (bitmap == (bitmap | 0x20)) return 5;
	if (bitmap == (bitmap | 0x40)) return 6;
	return 7;
}


/**********************************************************************
														 INT_HANDLER

	Loaded upon arrival of an interrupt, scans sequentially, thus
	guaranteeing priority, the lines, from those with lower index (faster)
	to those with higher index (slower).
	This is done based on the IP bits of the CAUSE register, then searching
	which device(s) among the 8 possible ones generated the interrupt.
	At this point the interrupt is handled, by sending a message to SSI
	and ACKing the device register, mainly, plus other actions depending
	on the specific device.

**********************************************************************/
void int_handler(){

	int int_cause;
	int *status;
	int *status_trans;
	int *status_rec;
	int *int_bitmap;
	int *command;
	int *command_trans;
	int *command_rec;
	memaddr device_baseaddr;
	int dev_number;

	/*
		If there was a process on the CPU, save the previous processor state in the
		t_state field of the tcb active at that moment.
		The state is saved in the old area of the current exception.
	*/
	if (current_thread != NULL) {

		/* Thread time update */
		current_thread->cpu_slice += (GET_TODLOW - current_thread_tod);
		current_thread->cpu_time += (GET_TODLOW - current_thread_tod);

		/* State save */
		save_state(int_oldarea, &(current_thread->t_state));
	}

	/* Retrieve cause register */
	int_cause = int_oldarea->cause;

	/* No interrupts from lines 0 and 1 (software) in AmiKaya11 */

	/* Line 2 Interval Timer Interrupt + PSEUDO CLOCK Management ****************************/
	if (CAUSE_IP_GET(int_cause, INT_TIMER)) {

		/* Pseudo clock update */
		pseudo_tick = pseudo_tick + (GET_TODLOW - start_pseudo_tick);
		start_pseudo_tick = GET_TODLOW;

		/* Interrupt for Pseudo-clock Tick */
		if (pseudo_tick >= SCHED_PSEUDO_CLOCK) {

			/* Message to SSI that will handle thread unblocking */
			interrupt_msg_array[i].service = PSEUDOCLOCK_MSG;
			interrupt_msg_array[i].arg = 0;
			interrupt_msg_array[i].reply = NOREPLY;

			if ( send((tcb_t *)BUS_INTERVALTIMER, SSI_tcb, (U32)&interrupt_msg_array[i]) == MSGNOGOOD ) PANIC();

			/* Restart the virtual device clock */
			pseudo_tick = 0;
			start_pseudo_tick = GET_TODLOW;

		}

		/* Management for expired current process Slice */
		if ((current_thread != NULL) && (current_thread->cpu_slice >= SCHED_TIME_SLICE)) {

			insertThread(&ready_queue, current_thread);
			current_thread = NULL;

		}

		/* Default */
		else SET_IT(SCHED_PSEUDO_CLOCK - pseudo_tick);

	} /* Interval Timer ********************************************************************/



	/* Line 3 Disk interrupt **************************************************************/
	else if (CAUSE_IP_GET(int_cause, INT_DISK)) {
		
		/* Search for the current line's bitmap */
		int_bitmap = (int *)(PENDING_BITMAP_START + (WORD_SIZE * (INT_DISK - INT_LOWEST)));

		/* Search for the highest priority device on this line with pending interrupt */
		dev_number = which_device(*int_bitmap);

		/* Save Device Register address */
		device_baseaddr = (memaddr)(DEV_REGS_START + ((INT_DISK - INT_LOWEST) * 0x80) + (dev_number * 0x10));

		/* Save value of Device Register Status field */
		status = (int *)device_baseaddr;

		/* Pointer to Device Register command field */
		command = (int *)(device_baseaddr + 0x4);

		/* ACK to device */
		*command = DEV_C_ACK;

		/* Send message to SSI, as sender the device register, as payload the status value */
		interrupt_msg_array[i].service = INTERRUPT_MSG;
		interrupt_msg_array[i].arg = *status;
		interrupt_msg_array[i].reply = NOREPLY;

		if ( send((tcb_t *)device_baseaddr, SSI_tcb, (U32)&interrupt_msg_array[i]) == MSGNOGOOD ) PANIC();
		
		
	} /* Disk ******************************************************************************/



	/* Line 4 Tape interrupt **************************************************************/
	else if (CAUSE_IP_GET(int_cause, INT_TAPE)) {


		/* Search for the current line's bitmap */
		int_bitmap = (int *)(PENDING_BITMAP_START + (WORD_SIZE * (INT_TAPE - INT_LOWEST)));

		/* Search for the highest priority device on this line with pending interrupt */
		dev_number = which_device(*int_bitmap);

		/* Save Device Register address */
		device_baseaddr = (memaddr)(DEV_REGS_START + ((INT_TAPE - INT_LOWEST) * 0x80) + (dev_number * 0x10));

		/* Save value of Device Register Status field */
		status = (int *)device_baseaddr;

		/* Pointer to Device Register command field */
		command = (int *)(device_baseaddr + 0x4);

		/* ACK to device */
		*command = DEV_C_ACK;

		/* Send message to SSI, as sender the device register, as payload the status value */
		interrupt_msg_array[i].service = INTERRUPT_MSG;
		interrupt_msg_array[i].arg = *status;
		interrupt_msg_array[i].reply = NOREPLY;

		if ( send((tcb_t *)device_baseaddr, SSI_tcb, (U32)&interrupt_msg_array[i]) == MSGNOGOOD ) PANIC();

	} /* Tape ******************************************************************************/



	/* Line 5 Unused line interrupt *******************************************************/
	else if (CAUSE_IP_GET(int_cause, INT_UNUSED)) {

		/* Search for the current line's bitmap */
		int_bitmap = (int *)(PENDING_BITMAP_START + (WORD_SIZE * (INT_UNUSED - INT_LOWEST)));

		/* Search for the highest priority device on this line with pending interrupt */
		dev_number = which_device(*int_bitmap);

		/* Save Device Register address */
		device_baseaddr = (memaddr)(DEV_REGS_START + ((INT_UNUSED - INT_LOWEST) * 0x80) + (dev_number * 0x10));

		/* Save value of Device Register Status field */
		status = (int *)device_baseaddr;

		/* Pointer to Device Register command field */
		command = (int *)(device_baseaddr + 0x4);

		/* ACK to device */
		*command = DEV_C_ACK;

		/* Send message to SSI, as sender the device register, as payload the status value */
		interrupt_msg_array[i].service = INTERRUPT_MSG;
		interrupt_msg_array[i].arg = *status;
		interrupt_msg_array[i].reply = NOREPLY;

		if ( send((tcb_t *)device_baseaddr, SSI_tcb, (U32)&interrupt_msg_array[i]) == MSGNOGOOD ) PANIC();

	} /* Unused ****************************************************************************/



	/* Line 6 Printer interrupt ***********************************************************/
	else if (CAUSE_IP_GET(int_cause, INT_PRINTER)) {

		/* Search for the current line's bitmap */
		int_bitmap = (int *)(PENDING_BITMAP_START + (WORD_SIZE * (INT_PRINTER - INT_LOWEST)));

		/* Search for the highest priority device on this line with pending interrupt */
		dev_number = which_device(*int_bitmap);

		/* Save Device Register address */
		device_baseaddr = (memaddr)(DEV_REGS_START + ((INT_PRINTER - INT_LOWEST) * 0x80) + (dev_number * 0x10));

		/* Save value of Device Register Status field */
		status = (int *)device_baseaddr;

		/* Pointer to Device Register command field */
		command = (int *)(device_baseaddr + 0x4);

		/* ACK to device */
		*command = DEV_C_ACK;

		/* Send message to SSI, as sender the device register, as payload the status value */
		interrupt_msg_array[i].service = INTERRUPT_MSG;
		interrupt_msg_array[i].arg = *status;
		interrupt_msg_array[i].reply = NOREPLY;

		if ( send((tcb_t *)device_baseaddr, SSI_tcb, (U32)&interrupt_msg_array[i]) == MSGNOGOOD ) PANIC();

	} /* Printer ***************************************************************************/



	/* Line 7 Terminal interrupt **********************************************************/
	else if (CAUSE_IP_GET(int_cause, INT_TERMINAL)) {

		/* Search for the current line's bitmap */
		int_bitmap = (int *)(PENDING_BITMAP_START + (WORD_SIZE * (INT_TERMINAL - INT_LOWEST)));

		/* Search for the highest priority device on this line with pending interrupt */
		dev_number = which_device(*int_bitmap);

		/* Save Device Register address */
		device_baseaddr = (memaddr)(DEV_REGS_START + ((INT_TERMINAL - INT_LOWEST) * 0x80) + (dev_number * 0x10));

		/* Save the state and pointer to the command field of the Device Register in transmission */
		status_trans = (int *)(device_baseaddr + 0x8);
		command_trans = (int *)(device_baseaddr + 0xC);

		/* Save the state and pointer to the command field of the Device Register in reception */
		status_rec = (int *)device_baseaddr;
		command_rec = (int *)(device_baseaddr + 0x4);

		/* Analyze the state to extract the cause of the interrupt and act accordingly */

		/* A character has been transmitted -> Priority to transmission */
		if (((*status_trans) & STATUSMASK) == DEV_TTRS_S_CHARTRSM)
		{
			/* Send message to SSI, as sender the device register, as payload the status value */
			interrupt_msg_array[i].service = INTERRUPT_MSG;
			interrupt_msg_array[i].arg = *status_trans;
			interrupt_msg_array[i].reply = NOREPLY;

			if ( send((tcb_t *)status_trans, SSI_tcb, (U32)&interrupt_msg_array[i]) == MSGNOGOOD ) PANIC();

			/* ACK to device */
			*command_trans = DEV_C_ACK;
		}

		/* A character has been received */
		else if (((*status_rec) & STATUSMASK) == DEV_TRCV_S_CHARRECV)
		{
			/* Send message to SSI, as sender the device register, as payload the status value */
			interrupt_msg_array[i].service = INTERRUPT_MSG;
			interrupt_msg_array[i].arg = *status_rec;
			interrupt_msg_array[i].reply = NOREPLY;

			if ( send((tcb_t *)status_rec, SSI_tcb, (U32)&interrupt_msg_array[i]) == MSGNOGOOD ) PANIC();

			/* ACK to device */
			*command_rec = DEV_C_ACK;
		}


	} /* Terminal **************************************************************************/

	/* Increment i in circular mode */
	i = (i+1)%48;
	
	scheduler();

}
