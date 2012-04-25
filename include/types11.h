#ifndef _TYPES11_H
#define _TYPES11_H
#include <uMPStypes.h>
#include <listx.h>
#include <const.h>

typedef struct tcb_t {
	/*process queue fields */

	struct list_head	t_next;

	/*process tree fields */
	struct tcb_t		*t_parent;
	struct list_head	t_child,
				t_sib;

	/* processor state, etc */
	state_t       t_state;     

	/*msg queue */
	struct list_head	t_inbox;

	/* Se sono fermo in wait_queue per una MsgRecv indica il thread da cui sto aspettando un messaggio */
	struct tcb_t *waiting_for;
	/* 	Se sono fermo in wait_queue per una MsgRecv indica dove voglio una risposta */
	U32 *reply;
	
	/* Specified trap management threads */
	struct tcb_t *sysbp_manager_thread;
	struct tcb_t *pgmtrap_manager_thread;
	struct tcb_t *tlbtrap_manager_thread;

	/* Tempo passato nella CPU */
	U32 cpu_time;
	/* Time slice attuale passato nella CPU */
	U32 cpu_slice;

	/*U32 pid;*/

} tcb_t;

typedef struct msg_t {
	/* msg queue */
	struct list_head	m_next;

	/* thread that sent this message */
	struct tcb_t    *m_sender;

	/* payload --> Modificato in U32 per congruenza con memaddr */
	U32			m_message;
	
	/* 
	Per messaggi SSI request 
	struct SSI_request_msg *m_SSIpayload; 
	*/

} msg_t;

#endif
