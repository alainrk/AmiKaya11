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

	Questo modulo implementa la routine di gestione degli interrupt, e la
	comunicazione a questo scopo con l'SSI.

************************************************************************/


/* Phase1 */
#include <msg.e>
#include <tcb.e>
/* Phase2 */
#include <boot.e>
#include <scheduler.e>
#include <exceptions.e>
#include <ssi.e>


/* Macro per leggibilità: riferimento a Old Area come stato del processore */
#define	int_oldarea	((state_t *)INT_OLDAREA)

/* Magic numbers per Interrupt a SSI */
#define INTERRUPT_MSG			255
#define PSEUDOCLOCK_MSG		254
#define NOREPLY						0


/* Array di strutture globale per messaggi interrupt a SSI */
struct SSI_request_msg interrupt_msg_array[48];
/* Usato per scorrere interrupt_msg_array[] in modo circolare */
HIDDEN int i=0;


/**********************************************************************
														 WHICH_DEVICE

	Individua l'indice del device prendendo come parametro la bitmap
	relativa alla linea dell'interrupt presa in considerazione.
	Soddisfa la priorità scandendo sequenzialmente dall'indice minore
	a quello maggiore.

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

	Caricato all'arrivo di un interrupt, scandisce sequenzialmente, quindi
	garantendo priorità, le	linee, da quelle con indice minore (più veloci)
	a quelle con indice maggiore (più lente).
	Ciò viene fatto in base ai bit IP del registro CAUSE, cercando poi 
	quale/i device fra gli 8 possibili ha generato l'interrupt.
	A questo punto viene gestito l'interrupt, mandando un messaggio a SSI
	e ACKando il device register, principalmente, più altre azioni a seconda 
	dello	specifico device.

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
		Se c'era un processo sulla CPU salvo il precedente stato del processore nel campo 
		t_state del tcb attivo in quel momento.
		Lo stato è salvato nella old area dell'attuale eccezione.
	*/
	if (current_thread != NULL) {		

		/* Aggiornamento tempo thread */
		current_thread->cpu_slice += (GET_TODLOW - current_thread_tod);
		current_thread->cpu_time += (GET_TODLOW - current_thread_tod);

		/* Salvataggio stato */
		save_state(int_oldarea, &(current_thread->t_state));
	}

	/* Prelevo registro cause */
	int_cause = int_oldarea->cause;
	
	/* Non presenti interrupt da linee 0 e 1 (software) in AmiKaya11 */

	/* Linea 2 Interval Timer Interrupt + Gestione PSEUDO CLOCK ****************************/
	if (CAUSE_IP_GET(int_cause, INT_TIMER)) {

		/* Aggiornamento pseudo clock */
		pseudo_tick = pseudo_tick + (GET_TODLOW - start_pseudo_tick);
		start_pseudo_tick = GET_TODLOW;

		/* Interrupt per Pseudo-clock Tick */
		if (pseudo_tick >= SCHED_PSEUDO_CLOCK) {

			/* Messaggio all'SSI che dovrà gestire lo sbloccaggio dei thread */			
			interrupt_msg_array[i].service = PSEUDOCLOCK_MSG;
			interrupt_msg_array[i].arg = 0;
			interrupt_msg_array[i].reply = NOREPLY;

			if ( send((tcb_t *)BUS_INTERVALTIMER, SSI_tcb, (U32)&interrupt_msg_array[i]) == MSGNOGOOD ) PANIC();

			/* Faccio ripartire il clock del device virtuale */				
			pseudo_tick = 0;
			start_pseudo_tick = GET_TODLOW;
			
		}

		/* Gestione per Slice processo corrente scaduto */
		if ((current_thread != NULL) && (current_thread->cpu_slice >= SCHED_TIME_SLICE)) {

			insertThread(&ready_queue, current_thread);
			current_thread = NULL;

		}

		/* Default */
		else SET_IT(SCHED_PSEUDO_CLOCK - pseudo_tick);
		
	} /* Interval Timer ********************************************************************/



	/* Linea 3 Disk interrupt **************************************************************/
	else if (CAUSE_IP_GET(int_cause, INT_DISK)) {
		
		/* Cerco la bitmap della linea attuale */
		int_bitmap = (int *)(PENDING_BITMAP_START + (WORD_SIZE * (INT_DISK - INT_LOWEST)));
		
		/* Cerco il device a più alta priorità su questa linea con interrupt pendente */
		dev_number = which_device(*int_bitmap);
		
		/* Salvo indirizzo del Device Register */
		device_baseaddr = (memaddr)(DEV_REGS_START + ((INT_DISK - INT_LOWEST) * 0x80) + (dev_number * 0x10));
		
		/* Salvo valore del campo Status del Device Register */
		status = (int *)device_baseaddr;

		/* Puntatore a campo command del Device Register */
		command = (int *)(device_baseaddr + 0x4);

		/* ACK al device */
		*command = DEV_C_ACK;
	
		/* Invio messaggio a SSI, come sender il device register, come payload il valore di status */
		interrupt_msg_array[i].service = INTERRUPT_MSG;
		interrupt_msg_array[i].arg = *status;
		interrupt_msg_array[i].reply = NOREPLY;

		if ( send((tcb_t *)device_baseaddr, SSI_tcb, (U32)&interrupt_msg_array[i]) == MSGNOGOOD ) PANIC();
		
		
	} /* Disk ******************************************************************************/



	/* Linea 4 Tape interrupt **************************************************************/
	else if (CAUSE_IP_GET(int_cause, INT_TAPE)) {


		/* Cerco la bitmap della linea attuale */
		int_bitmap = (int *)(PENDING_BITMAP_START + (WORD_SIZE * (INT_TAPE - INT_LOWEST)));
		
		/* Cerco il device a più alta priorità su questa linea con interrupt pendente */
		dev_number = which_device(*int_bitmap);
		
		/* Salvo indirizzo del Device Register */
		device_baseaddr = (memaddr)(DEV_REGS_START + ((INT_TAPE - INT_LOWEST) * 0x80) + (dev_number * 0x10));
		
		/* Salvo valore del campo Status del Device Register */
		status = (int *)device_baseaddr;

		/* Puntatore a campo command del Device Register */
		command = (int *)(device_baseaddr + 0x4);

		/* ACK al device */
		*command = DEV_C_ACK;
	
		/* Invio messaggio a SSI, come sender il device register, come payload il valore di status */
		interrupt_msg_array[i].service = INTERRUPT_MSG;
		interrupt_msg_array[i].arg = *status;
		interrupt_msg_array[i].reply = NOREPLY;

		if ( send((tcb_t *)device_baseaddr, SSI_tcb, (U32)&interrupt_msg_array[i]) == MSGNOGOOD ) PANIC();
		
	} /* Tape ******************************************************************************/



	/* Linea 5 Unused line interrupt *******************************************************/
	else if (CAUSE_IP_GET(int_cause, INT_UNUSED)) {

		/* Cerco la bitmap della linea attuale */
		int_bitmap = (int *)(PENDING_BITMAP_START + (WORD_SIZE * (INT_UNUSED - INT_LOWEST)));
		
		/* Cerco il device a più alta priorità su questa linea con interrupt pendente */
		dev_number = which_device(*int_bitmap);
		
		/* Salvo indirizzo del Device Register */
		device_baseaddr = (memaddr)(DEV_REGS_START + ((INT_UNUSED - INT_LOWEST) * 0x80) + (dev_number * 0x10));
		
		/* Salvo valore del campo Status del Device Register */
		status = (int *)device_baseaddr;

		/* Puntatore a campo command del Device Register */
		command = (int *)(device_baseaddr + 0x4);

		/* ACK al device */
		*command = DEV_C_ACK;
	
		/* Invio messaggio a SSI, come sender il device register, come payload il valore di status */
		interrupt_msg_array[i].service = INTERRUPT_MSG;
		interrupt_msg_array[i].arg = *status;
		interrupt_msg_array[i].reply = NOREPLY;

		if ( send((tcb_t *)device_baseaddr, SSI_tcb, (U32)&interrupt_msg_array[i]) == MSGNOGOOD ) PANIC();
		
	} /* Unused ****************************************************************************/



	/* Linea 6 Printer interrupt ***********************************************************/
	else if (CAUSE_IP_GET(int_cause, INT_PRINTER)) {

		/* Cerco la bitmap della linea attuale */
		int_bitmap = (int *)(PENDING_BITMAP_START + (WORD_SIZE * (INT_PRINTER - INT_LOWEST)));
		
		/* Cerco il device a più alta priorità su questa linea con interrupt pendente */
		dev_number = which_device(*int_bitmap);
		
		/* Salvo indirizzo del Device Register */
		device_baseaddr = (memaddr)(DEV_REGS_START + ((INT_PRINTER - INT_LOWEST) * 0x80) + (dev_number * 0x10));
		
		/* Salvo valore del campo Status del Device Register */
		status = (int *)device_baseaddr;

		/* Puntatore a campo command del Device Register */
		command = (int *)(device_baseaddr + 0x4);

		/* ACK al device */
		*command = DEV_C_ACK;
	
		/* Invio messaggio a SSI, come sender il device register, come payload il valore di status */
		interrupt_msg_array[i].service = INTERRUPT_MSG;
		interrupt_msg_array[i].arg = *status;
		interrupt_msg_array[i].reply = NOREPLY;

		if ( send((tcb_t *)device_baseaddr, SSI_tcb, (U32)&interrupt_msg_array[i]) == MSGNOGOOD ) PANIC();
		
	} /* Printer ***************************************************************************/



	/* Linea 7 Terminal interrupt **********************************************************/
	else if (CAUSE_IP_GET(int_cause, INT_TERMINAL)) {

		/* Cerco la bitmap della linea attuale */
		int_bitmap = (int *)(PENDING_BITMAP_START + (WORD_SIZE * (INT_TERMINAL - INT_LOWEST)));
		
		/* Cerco il device a più alta priorità su questa linea con interrupt pendente */
		dev_number = which_device(*int_bitmap);
		
		/* Salvo indirizzo del Device Register */
		device_baseaddr = (memaddr)(DEV_REGS_START + ((INT_TERMINAL - INT_LOWEST) * 0x80) + (dev_number * 0x10));

		/* Salvo lo stato e il puntatore al campo command del Device Register in trasmissione */
		status_trans = (int *)(device_baseaddr + 0x8);
		command_trans = (int *)(device_baseaddr + 0xC);

		/* Salvo lo stato e il puntatore al campo command del Device Register in ricezione */
		status_rec = (int *)device_baseaddr;
		command_rec = (int *)(device_baseaddr + 0x4);

		/* Analizzo lo stato per estrarre la causa dell'interrupt e agisco di conseguenza*/

		/* Un carattere è stato trasmesso -> Priorità alla trasmissione */
		if (((*status_trans) & STATUSMASK) == DEV_TTRS_S_CHARTRSM)
		{
			/* Invio messaggio a SSI, come sender il device register, come payload il valore di status */
			interrupt_msg_array[i].service = INTERRUPT_MSG;
			interrupt_msg_array[i].arg = *status_trans;
			interrupt_msg_array[i].reply = NOREPLY;

			if ( send((tcb_t *)status_trans, SSI_tcb, (U32)&interrupt_msg_array[i]) == MSGNOGOOD ) PANIC();

			/* ACK al device */
			*command_trans = DEV_C_ACK;
		}
		
		/* Un carattere è stato ricevuto */
		else if (((*status_rec) & STATUSMASK) == DEV_TRCV_S_CHARRECV)
		{
			/* Invio messaggio a SSI, come sender il device register, come payload il valore di status */
			interrupt_msg_array[i].service = INTERRUPT_MSG;
			interrupt_msg_array[i].arg = *status_rec;
			interrupt_msg_array[i].reply = NOREPLY;

			if ( send((tcb_t *)status_rec, SSI_tcb, (U32)&interrupt_msg_array[i]) == MSGNOGOOD ) PANIC();

			/* ACK al device */
			*command_rec = DEV_C_ACK;
		}
		
		
	} /* Terminal **************************************************************************/

	/* Incremento i in modo circolare */
	i = (i+1)%48;
	
	scheduler();

}
