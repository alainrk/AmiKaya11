                                              
/*
 * Copyright (C) Michael Goldweber, Enrico Cataldi, Mauro Morsiani
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*********************************P2TEST.C*******************************
 *
 *      Test program for the AMIKaya Kernel: phase 2.
 *
 *      Produces progress messages on Terminal0.
 *
 *      This is pretty convoluted code, so good luck!
 *
 *              Aborts as soon as an error is detected.
 *
 */
 
#include "const.h"
#include "types11.h"
#include "const11.h"

/*****************************FIX DIFFERENT NAMES*********************************/
#define s_sp reg_sp
#define s_pc pc_epc
#define s_t9 reg_t9
#define s_status status
#define cpu_t U32
#define proc_state t_state
#define s_a0 reg_a0
#define DEVPERINT DEV_PER_INT
/*********************************************************************************/


/*	 p2 tests constants	*/
#define MINLOOPTIME	30000
#define LOOPNUM 	10000

/*	 p3 tests constants	*/
#define CLOCKLOOP	10
#define MINCLOCKLOOP	3000	
#define CLOCKINTERVAL 100000

/*	 p5 and p9 tests constants	*/
#define SYSCAUSE	(0x8 << 2)
#define ADDRCAUSE	(0x6 << 2)
#define UPLEVELSYS 	3
#define LOCATIONADDR	0x40000000
#define BADADDR	0xFFFFFFFF
/*	 p8 tests constants	*/
#define P8LOOP		4
#define NOLEAVES	4	

/* sync code used by p1 and p2 to acknowledge each other */
#define SYNCCODE	0x10101010 
                                                                                
/* Status register constants */
#define ALLOFF                  0x00000000
#define VMPON                   0x02000000
#define USERPON                 0x00000008
#define IEPON                   0x00000004

#define IMALLON                 0x0000FF00 /* This is a mask to set that all device interrupts are active for the thread */

#define QPAGE                   1024

state_t		p2state, p3state,p4state, p5state,p6state, p7state,p8rootstate, child1state,
		child2state, gchild1state,gchild2state, gchild3state,gchild4state,
		p9state, printstate, p5trap_s, p9trap_s;

HIDDEN int		p1p2synch,		/* to check on p1/p2 synchronization */
			p8inc,		/* p8's incarnation number */ 
			p4inc=1;		/* p4 incarnation number */

/* to get some fresh stack space for the second p5 thread */
memaddr p5Stack; 

/* Threads main funcions */
void test(void);
HIDDEN void p2(void);
HIDDEN void p3(void);
HIDDEN void p4(void);
HIDDEN void p5(void);
HIDDEN void p5a(void);
HIDDEN void p6(void);
HIDDEN void p7(void);
HIDDEN void p5trapm(void);
HIDDEN void p8root(void);
HIDDEN void child1(void);
HIDDEN void child2(void);
HIDDEN void p8leaf(void);
HIDDEN void p9(void);
HIDDEN void p9trapm(void);
HIDDEN void printthread(void);

/* Local functions */
HIDDEN void print(char * message);
HIDDEN unsigned int termprint(char * str, unsigned int term);
HIDDEN void putChar(memaddr *command, char ch);

/* Local vars */
HIDDEN tcb_t *printid, *p2id, *p3id, *p4id, *p5id, *p6id, *p7id, *p8id, *p9id;


tcb_t *feedBack;


/*                 p1 -- the root process                            */
void test(void){	
	unsigned int reply;
	char *tmp;


	p1p2synch = 0;
	p8inc = 0;
	p4inc = 1;

	/* Storing actual state */
	STST(&printstate);

	/* stack of printthread should sit below*/
	printstate.s_sp = printstate.s_sp - QPAGE;
	/* printthread starts executing his code */
	printstate.s_pc = printstate.s_t9 = (memaddr)printthread;
	/* Here we set that IEp is on */
	printstate.s_status = printstate.s_status | IEPON | IMALLON;

	SSIRequest(CREATESON, (unsigned int) &printstate, &reply);

	if (reply == CREATENOGOOD)
		/* could not start print terminal thread */
		PANIC();

	printid=(tcb_t *)reply;
	tmp="print terminal thread has started\n";

	print(tmp);

	/* set up states of the other processes */

	/* set up p2's state */
	STST(&p2state);			
	p2state.s_sp = printstate.s_sp - QPAGE;
	p2state.s_pc = p2state.s_t9 = (memaddr)p2;
	p2state.s_status = p2state.s_status | IEPON | IMALLON;

	STST(&p3state);
	p3state.s_sp = p2state.s_sp - QPAGE;
	p3state.s_pc = p3state.s_t9 = (memaddr)p3;
	p3state.s_status = p3state.s_status | IEPON | IMALLON;

	STST(&p4state);
	p4state.s_sp = p3state.s_sp - QPAGE;
	p4state.s_pc = p4state.s_t9 = (memaddr)p4;
	p4state.s_status = p4state.s_status | IEPON | IMALLON;

	STST(&p5state);
	p5Stack = p5state.s_sp = p4state.s_sp - (2 * QPAGE);
	p5state.s_pc = p5state.s_t9 = (memaddr)p5;
	p5state.s_status = p5state.s_status | IEPON | IMALLON;

	STST(&p5trap_s);
	p5trap_s.s_sp = p5state.s_sp - (3 * QPAGE);
	p5trap_s.s_pc = p5trap_s.s_t9 = (memaddr) p5trapm;
	p5trap_s.s_status = p5trap_s.s_status | IEPON | IMALLON;

	STST(&p6state);
	p6state.s_sp = p5trap_s.s_sp - QPAGE;
	p6state.s_pc = p6state.s_t9 = (memaddr)p6;
	p6state.s_status = p6state.s_status | IEPON | IMALLON;

	STST(&p7state);
	p7state.s_sp = p6state.s_sp - QPAGE;
	p7state.s_pc = p7state.s_t9 = (memaddr)p7;
	p7state.s_status = p7state.s_status | IEPON | IMALLON;

	STST(&p8rootstate);
	p8rootstate.s_sp = p7state.s_sp - QPAGE;
	p8rootstate.s_pc = p8rootstate.s_t9 = (memaddr)p8root;
	p8rootstate.s_status = p8rootstate.s_status | IEPON | IMALLON;

	STST(&child1state);
	child1state.s_sp = p8rootstate.s_sp - QPAGE;
	child1state.s_pc = child1state.s_t9 = (memaddr)child1;
	child1state.s_status = child1state.s_status | IEPON | IMALLON;

	STST(&child2state);
	child2state.s_sp = child1state.s_sp - QPAGE;
	child2state.s_pc = child2state.s_t9 = (memaddr)child2;
	child2state.s_status = child2state.s_status | IEPON | IMALLON;

	STST(&gchild1state);
	gchild1state.s_sp = child2state.s_sp - QPAGE;
	gchild1state.s_pc = gchild1state.s_t9 = (memaddr)p8leaf;
	gchild1state.s_status = gchild1state.s_status | IEPON | IMALLON;

	STST(&gchild2state);
	gchild2state.s_sp = gchild1state.s_sp - QPAGE;
	gchild2state.s_pc = gchild2state.s_t9 = (memaddr)p8leaf;
	gchild2state.s_status = gchild2state.s_status | IEPON | IMALLON;

	STST(&gchild3state);
	gchild3state.s_sp = gchild2state.s_sp - QPAGE;
	gchild3state.s_pc = gchild3state.s_t9 = (memaddr)p8leaf;
	gchild3state.s_status = gchild3state.s_status | IEPON | IMALLON;

	STST(&gchild4state);
	gchild4state.s_sp = gchild3state.s_sp - QPAGE;
	gchild4state.s_pc = gchild4state.s_t9 = (memaddr)p8leaf;
	gchild4state.s_status = gchild4state.s_status | IEPON | IMALLON;

	STST(&p9state);
	p9state.s_sp = gchild4state.s_sp - QPAGE;
	p9state.s_pc = p9state.s_t9 = (memaddr)p9;
	p9state.s_status = p9state.s_status | IEPON | IMALLON;

	STST(&p9trap_s);
	p9trap_s.s_sp = p9state.s_sp - QPAGE;
	p9trap_s.s_pc = p9trap_s.s_t9 = (memaddr) p9trapm;
	p9trap_s.s_status = p9trap_s.s_status | IEPON | IMALLON;

	print ("p1 setup ok: now starting the tests\n");

	/*		 start p2 thread	*/
	SSIRequest(CREATEBROTHER, (unsigned int) &p2state, (unsigned int *)&p2id);

	if ((int)p2id == CREATENOGOOD)
		print("error: cannot create brother p2\n");
	else
		print("p2 started\n");

	/*	 check p1/p2 message syncronization	*/
	MsgSend(SEND, p2id, SYNCCODE);	

	/*	 wait for p2 to end	*/
	MsgRecv(RECV, p2id, &reply);

	/*		 make sure they really synced	*/
	if (p1p2synch == 0)
		print("error: p1/p2 synchronization bad\n");
	else
		print("p2 provide a good synch\n");

	/*		 start p3 thread	*/
	SSIRequest(CREATEBROTHER, (unsigned int) &p3state, (unsigned int *)&p3id);

	print("p3 started\n");

	MsgSend(SEND, p3id, 0); /* start p3 tests */

	/*		 wait for p3 to end its tests	*/
	MsgRecv(RECV, p3id, &reply);

	/*		 start first incarnation of p4	*/
	SSIRequest(CREATEBROTHER, (unsigned int) &p4state, (unsigned int *)&p4id);

	/*	 	 start p4 tests	*/
	MsgSend(SEND, p4id, 0);	

	/*		 wait for p4 first incarnation	*/
	MsgRecv(RECV, p4id, &reply);

	/*		 ask p4 to spawn another thread	*/
	MsgSend(SEND, p4id, 0);

	/*		 start p5 thread	*/
	SSIRequest(CREATEBROTHER, (unsigned int) &p5state, (unsigned int *)&p5id);

	/*		 start p5 tests	*/
	MsgSend(SEND, p5id, 0);

	/*		 start p6 thread	*/
	SSIRequest(CREATEBROTHER, (unsigned int) &p6state, (unsigned int *)&p6id);

	/*		 start p6 tests	*/
	MsgSend(SEND, p6id, 0);

	/*		 start p7 thread	*/
	SSIRequest(CREATEBROTHER, (unsigned int) &p7state, (unsigned int *)&p7id);

	/*		 start p7 tests	*/
	MsgSend(SEND, p7id, 0);

	/*		 wait for p4 to end	*/
	MsgRecv(RECV, p4id, &reply);

	/*		 this should signal p5a to continue	*/
	MsgSend(SEND, p5id, 0);	

	/*		 wait for p5a to complete handshake	*/
	MsgRecv(RECV, p5id, &reply);

	/*		 start p9 thread	*/
	SSIRequest(CREATEBROTHER, (unsigned int) &p9state, (unsigned int *)&p9id);

	/*		 start p9 tests	*/
	MsgSend(SEND, p9id, 0);

	/*		 now a more rigorous check of process creation/termination 	*/
	for (p8inc = 0; p8inc < P8LOOP; p8inc++){
		SSIRequest(CREATEBROTHER, (unsigned int) &p8rootstate, (unsigned int *)&p8id);

		if ((int) (p8id) == CREATENOGOOD){
			print("error in thread creation\n");
			PANIC();
		}

		/*			 else start p8 tests 	*/
		MsgSend(SEND, p8id, 0);

		/*			 wait for p8 to end	*/
		MsgRecv(RECV, p8id, &reply);
	}

	print("p1 finishes OK\n");
	* ((memaddr *) BADADDR) = 0;				/* terminate p1 */

	/* should not reach this point, since p1 just got a program trap */
	print("error: p1 still alive after progtrap & no trap vector\n");
	PANIC();
}


/* p2 -- sync and cputime test thread */
void p2(void)
{
	int	i;			/* just to waste time  */
	unsigned int 	reply;
	tcb_t * parent;
	cpu_t	now1,now2;		/* times of day        */
	cpu_t	cpu_t1,cpu_t2;		/* cpu time used       */


	parent = MsgRecv(RECV, ANYMESSAGE, &reply);

	if (reply != SYNCCODE){
		print("error: p2 received a bad message\n");
		PANIC();
	}

	print("p2 received right message from p1\n");

	STCK(now1);				/* time of day   */
	SSIRequest(GETCPUTIME, 0, &reply);


	/* CPU time used until now*/
	cpu_t1 = (cpu_t) reply;

	/* delay for several milliseconds */
	for (i=1; i < LOOPNUM; i++);

	SSIRequest(GETCPUTIME, 0, &reply);

	cpu_t2 = (cpu_t) reply;
	STCK(now2);/* time of day  */

	if (((now2 - now1) >= (cpu_t2 - cpu_t1)) && ((cpu_t2 - cpu_t1) >= (MINLOOPTIME / (* ((cpu_t *)BUS_TIMESCALE)))))
		print("p2 is OK\n");
	else  {
		if ((now2 - now1) < (cpu_t2 - cpu_t1))
			print("error: more cpu time than real time\n");
		if ((cpu_t2 - cpu_t1) < (MINLOOPTIME / (* ((cpu_t *)BUS_TIMESCALE))))
			print("error: not enough cpu time went by\n");
		print("error: p2 blew it!\n");
	}

	p1p2synch = 1; /* p1 will check this */

	/*		 restart p1	*/
	MsgSend(SEND, parent, 0);

	SSIRequest(TERMINATE, 0, &reply);	/* terminate p2 */

	/* just did a TERMINATE, so should not get to this point */
	print("error: p2 didn't terminate\n");
	PANIC();
}


/* p3 -- waitclock test thread */
void p3(void)
{
	int	i;
	unsigned int reply;
	tcb_t *parent;
	cpu_t	time1,time2;
	cpu_t	cpu_t1,cpu_t2;		/* cpu time used       */


	time1=0;
	time2=0;

	parent = MsgRecv(RECV, ANYMESSAGE, &reply);

	/* loop until we are delayed at least half of clock V interval */
	while (time2 - time1 < (CLOCKINTERVAL >> 1) ) {
		STCK(time1);			/* time of day     */
		SSIRequest(WAITFORCLOCK, 0, &reply);
		STCK(time2);			/* new time of day */
	}

	print("p3: WAITFORCLOCK looks OK: now a longer test\n");

	/* now let's check to see if we're really charge for CPU
	   time correctly */
	SSIRequest(GETCPUTIME, 0, &reply);
	cpu_t1 = (cpu_t) reply;

	for (i = 0; i < CLOCKLOOP; i++)
		SSIRequest(WAITFORCLOCK, 0, &reply);

	SSIRequest(GETCPUTIME, 0, &reply);
	cpu_t2 = (cpu_t) reply;

	if (cpu_t2 - cpu_t1 < (MINCLOCKLOOP / (* ((cpu_t *) BUS_TIMESCALE))))
		print("error: p3: CPU time incorrectly maintained\n");
	else
		print("p3: CPU time correctly maintained\n");

	/* restart p1 	*/
	MsgSend(SEND, parent, 0);

	SSIRequest(TERMINATE, 0, &reply);			/* terminate p3    */

	/* just did a TERMINATE, so should not get to this point */
	print("error: p3 didn't terminate\n");
	PANIC(); 
}


/* p4 -- termination test process */
void p4(void)
{
	unsigned int reply;
	tcb_t *parent, *son;

	parent = MsgRecv(RECV, ANYMESSAGE, &reply);

	switch (p4inc) {
		case 1:
			print("p4: first incarnation starts\n");
			p4inc++;
			break;
		case 2:
			print("p4: second incarnation starts\n");
			break;
	}
	/*		 notify p1 (or p4 first incarnation)	*/

	MsgSend(SEND, parent, 0);

	/*		 wait parent authorization to start a new thread;	*/
	/*		 p4 second incarnation will wait here forever	if no answer */
	MsgRecv(RECV, parent, &reply);

	p4state.s_sp =   p4state.s_sp - QPAGE;		/* give some stack space  */

	SSIRequest(CREATESON, (unsigned int) &p4state, &reply); /* start a new p4    */
	son = (tcb_t *) reply;

	if ((int)son == CREATENOGOOD){
		print("error: p4 cannot create the second p4\n");
		PANIC();
	}

	MsgSend(SEND, son, 0);			/* start p4 second incarnation  */
	MsgRecv(RECV, son, &reply);			/* wait for it       */

	print("p4 is OK\n");

	/* restart p1 (p4 second incarnation will never reach this point)	*/
	MsgSend(SEND, parent, 0);

	/* this termination request will kill both p4 incarnations	*/
	SSIRequest(TERMINATE, 0, &reply);

	/* just did a TERMINATE, so should not get to this point */
	print("error: p4 didn't terminate\n");
	PANIC(); /* PANIC */
}


/* p5 trap manager; will be terminated when p5 dies */
void p5trapm(void)
{
	unsigned int reply;
	tcb_t *trapped;

	print("p5 trap mgr started\n");

	for (;;){
		/* wait for a trap to happen	*/
		trapped = MsgRecv(RECV, ANYMESSAGE, &reply);

		switch(reply & CAUSEMASK){

			case SYSCAUSE:
				if (trapped->proc_state.s_a0 == UPLEVELSYS){
					print("p5 trap mgr: got up-level SYSCALL passup\n");
					trapped->proc_state.s_pc += 4; /* to avoid SYS looping */
				}else{
					/* should be the MsgRecv done in p5a, user mode */
					print("p5 trap mgr: Kernel mode required for MsgRecv(): retrying\n");
					/* restart p5 from p5a(), kernel mode	*/
					trapped->proc_state.s_status = trapped->proc_state.s_status & (~((unsigned int) USERPON));
					trapped->proc_state.s_pc = trapped->proc_state.s_t9 = (memaddr)p5a;	
					/* some fresh stack for it */
					trapped->proc_state.s_sp = p5Stack - (2 * QPAGE);
				}
				break;
			case ADDRCAUSE:
				print("p5 trap mgr: TLB trap; restarting as p5a, User mode\n");
				/* restart p5 from p5a(), user mode	*/
				trapped->proc_state.s_pc = trapped->proc_state.s_t9 = (memaddr)p5a;
				trapped->proc_state.s_status = trapped->proc_state.s_status | USERPON | IEPON;
				/* some fresh stack for it */
				trapped->proc_state.s_sp = p5Stack - QPAGE;
				break;

			default:
				/* an unexpected error has happened	*/
				print("error: unexpected trap in p5\n");
				PANIC();
				break;
		}
		MsgSend(SEND, trapped, TRAPCONTINUE);
	}		
}


/* p5 -- trap management test process */
void p5(void)
{
	memaddr location = 0;

	unsigned int reply;
	tcb_t * p5trap_m;

	/* syncs with p1 */
	MsgRecv(RECV, ANYMESSAGE, &reply);
	print("p5 starts\n");

	/* setup & start p5 trap management thread */
	SSIRequest(CREATESON, (unsigned int) &p5trap_s, &reply);
	p5trap_m = (tcb_t *) reply;

	/* specify trap manager thread for p5 */
	SSIRequest(SPECPRGMGR, (unsigned int) p5trap_m, &reply);
	SSIRequest(SPECTLBMGR, (unsigned int) p5trap_m, &reply);
	SSIRequest(SPECSYSMGR, (unsigned int) p5trap_m, &reply);

	/* this SYSCALL should be handled by p5trap_m */
	SYSCALL(UPLEVELSYS,0,0,0);

	print("p5 returns after up-level syscall\n");

	/* to cause a TLB trap access a valid but not-used kuseg location is enough */
	
	location = * ((memaddr *) LOCATIONADDR);

	/* after this, the trap manager will load p5a in user mode, to test other handlers */
}

/* second part of p5 - should be entered in user mode */
void p5a(void)
{
	unsigned int reply;
	tcb_t *thread;

	/* the first time through, we are in user mode */
	/* and the MsgRecv() should generate a program trap */
	thread = MsgRecv(RECV, ANYMESSAGE, &reply);

	/* authorizes p1 to continue */
	MsgSend(SEND, thread, 0);

	print("p5a handshake with p1 OK\n");

	/* should cause a termination for p5 and his trap mgr     
	   since this SSIRequest has already been done for p5 */                 
	SSIRequest(SPECPRGMGR, (unsigned int) NULL, &reply);

	/* should have terminated, so should not get to this point */
	print("error: p5 didn't terminate\n");
	PANIC();
}


/*p6 -- high level syscall without initializing trap vector*/
void p6(void)
{
	unsigned int reply;

	/* waiting for a message from p1 */
	MsgRecv(RECV, ANYMESSAGE, &reply);

	print("p6 starts\n");

	/* should cause termination because p6 has no trap mgr*/
	SYSCALL(UPLEVELSYS,0,0,0);		

	print("error: p6 alive after a passup with no trap vector\n");
	PANIC();
}

/*p7 -- program trap without initializing passup vector*/
void p7(void)
{
	unsigned int reply;
	tcb_t *thread;

	/*		 waiting for a message from p1	*/
	thread = MsgRecv(RECV, ANYMESSAGE, &reply);

	print("p7 starts\n");

	* ((memaddr *) BADADDR) = 0;

	print("error: p7 alive after program trap with no trap vector\n");
	PANIC();
}


/* p8root -- test of termination of a subtree of threads              */
/* create a subtree of threads, wait for the leaves to block, signal  */
/* the parent process (p1), and then terminate                        */
void p8root(void)
{
	unsigned int reply;
	tcb_t *parent,*tmp;
	int	grandchild;

	print("p8root starts\n");

	/* sync with p1	*/
	parent = MsgRecv(RECV, ANYMESSAGE, &reply);

	/*		 start sons		*/
	SSIRequest(CREATESON, (unsigned int) &child1state, (unsigned int *)&tmp);
	MsgSend(SEND, tmp, 0);

	SSIRequest(CREATESON, (unsigned int) &child2state, (unsigned int *)&tmp);
	MsgSend(SEND, tmp, 0);

	/* wait for leaves 		*/
	for (grandchild = 0; grandchild < NOLEAVES; grandchild++)
		feedBack=MsgRecv(RECV, ANYMESSAGE, &reply);

	/* signal p1 to continue	*/
	MsgSend(SEND, parent, 0);

	SSIRequest(TERMINATE, 0, &reply);
}

/*child1 & child2 -- create two sub-processes each*/

void child1(void)
{
	unsigned int reply; 
	tcb_t *parent;

	print("child1 starts\n");

	/* syncs with p8root	*/
	parent = MsgRecv(RECV, ANYMESSAGE, &reply);

	/* creates sons and passes them p8root address	*/
	SSIRequest(CREATESON, (unsigned int) &gchild1state, &reply);
	MsgSend(SEND, (tcb_t *) reply, (unsigned int) parent);

	SSIRequest(CREATESON, (unsigned int) &gchild2state, &reply);
	MsgSend(SEND, (tcb_t *) reply, (unsigned int) parent);

	/* wait forever	*/
	MsgRecv(RECV, ANYMESSAGE, &reply);

	/*		 should never get there: nobody should send message to childs	*/
	print("error: child1 received message from someone\n");
	PANIC();
}

void child2(void)
{
	unsigned int reply;
	tcb_t *parent;

	print("child2 starts\n");

	/*	 syncs with p8root	*/
	parent = MsgRecv(RECV, ANYMESSAGE, &reply);

	/*	 creates sons and passes them p8root address	*/
	SSIRequest(CREATESON, (unsigned int) &gchild3state, &reply);
	MsgSend(SEND, (tcb_t *) reply, (unsigned int) parent);

	SSIRequest(CREATESON, (unsigned int) &gchild4state, &reply);
	MsgSend(SEND, (tcb_t *) reply, (unsigned int) parent);

	/*		 wait forever	*/
	MsgRecv(RECV, ANYMESSAGE, &reply);

	/*		 should never get there: nobody should send message to childs	*/
	print("error: child2 received message from someone\n");
	PANIC();
}

/*p8leaf -- code for leaf processes*/
void p8leaf()
{
	tcb_t * tmp;
	unsigned int reply;

	print("leaf process starts\n");
	/* syncs with child */
	MsgRecv(RECV, ANYMESSAGE, (unsigned int *)&tmp);
	MsgSend(SEND, tmp, 0);

	/*		 wait forever	*/
	MsgRecv(RECV, ANYMESSAGE, &reply);

	/*		 should never get there: nobody should send message to leafs	*/
	print("error: leaf received message from someone\n");
	PANIC();
}


/* p9 trap manager; should be terminated when p9 dies */
void p9trapm(void)
{
	unsigned int reply;
	tcb_t *trapped;

	print("p9 trap mgr started\n");

	for(;;) {
		/*	 wait for a trap to happen	*/
		trapped = MsgRecv(RECV, ANYMESSAGE, &reply);

		if ((reply & CAUSEMASK) == SYSCAUSE && trapped->proc_state.s_a0 == UPLEVELSYS){
			print("SYSCALL in p9: terminate it\n");
			MsgSend(SEND, trapped, TRAPTERMINATE);
		} else {
			print("error: unexpected trap in p9\n");
			PANIC();
		}
	}		
}


/* p9 -- a simpler trap management test process */
void p9(void)
{
	unsigned int reply;
	tcb_t *p9trap_m;

	/*	 syncs with p1	*/
	MsgRecv(RECV, ANYMESSAGE, &reply);

	print("p9 starts\n");

	/*		 setup & start p9 trap management thread	*/
	SSIRequest(CREATESON, (unsigned int) &p9trap_s, &reply);
	p9trap_m = (tcb_t *) reply;

	/* specify trap manager thread for p9 */
	SSIRequest(SPECPRGMGR, (unsigned int) p9trap_m, &reply);
	SSIRequest(SPECTLBMGR, (unsigned int) p9trap_m, &reply);
	SSIRequest(SPECSYSMGR, (unsigned int) p9trap_m, &reply);

	/*		 this SYSCALL should be handled by p9trap_m	*/
	/*		 which should terminate 	*/
	SYSCALL(UPLEVELSYS,0,0,0);

	/* should have terminated, so should not get to this point */
	print("error: p9 didn't terminate\n");
	PANIC();
}



/* This procedure will send a message to the terminal printer thread */
void print(char * message){
	unsigned int reply;
	MsgSend(SEND, printid, (unsigned int) message);
	MsgRecv(RECV, printid, &reply);
}


/* a RPC server thread which prints messages on terminal 0 */
void printthread(void)
{	
	unsigned int payload;
	tcb_t *sender;


	for(;;){
		/* waits for a job */
		sender = MsgRecv(RECV, ANYMESSAGE, &payload);

		termprint((char *) payload,0);	

		/* job done: client thread may restart	*/
		if ((MsgSend(SEND, sender, 0)) == MSGNOGOOD)
		{
			termprint("print thread cannot give back an answer\n",0);
			PANIC();
		}
	}
}


/* This function prints a string on specified terminal and returns TRUE if print was successful, FALSE if not  */
unsigned int termprint(char *inpstr, unsigned int term){
	memaddr *statusp;
	memaddr *commandp;
	char *str;
	unsigned int stat;
	unsigned int error;

	error = FALSE;
	str=inpstr;
	/* IF the term number is correct */
	if (term < DEVPERINT) {
		/* Compute device register field addresses */
		statusp =  (unsigned int *) (TERMINAL0ADDR + (term * DEV_REG_SIZE) + (TRANSTATUS * DEV_REG_LEN));
		commandp = (unsigned int *) (TERMINAL0ADDR + (term * DEV_REG_SIZE) + (TRANCOMMAND * DEV_REG_LEN));

		/* Retrieve device status */
		stat = ((*statusp) & STATUSMASK);

		/* If device is available */
		if (stat == READY || stat == TRANSMITTED) {

			/* During this cycle we will print one char a time*/
			while (*str != EOS && !error) {
				/* Now we shift the char to be printed by CHAROFFSET to 
				 * put its bits to the 8 higher bits and set the command code at 2*/
				putChar(commandp,*str);

				/* Waiting for IO Completion */
				SSIRequest(WAITFORIO,(unsigned int)statusp,(unsigned int *)&stat);

				if (stat != TRANSMITTED)
					error = TRUE;
				else{
					/* Move to next char */
					str++;
					*commandp = ACK;
				}
			}
		}
		else
			/* Device is not available */
			error = TRUE;
	}
	else
		/* Wrong terminal device number */
		error = TRUE;

	*commandp = ACK;

	return (!error);
}


HIDDEN void putChar(memaddr *command, char ch){
	*command = (ch << CHAROFFSET) | PRINTCHAR;
}
