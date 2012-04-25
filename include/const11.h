#ifndef CONST11_H
#define CONST11_H

#define MAXTHREADS 20
#define MAXMESSAGES 20

/* Syscall mnemonic values */
#define SEND            1
#define RECV            2

/* service request values */
#define CREATEBROTHER   1
#define CREATESON       2
#define TERMINATE       3
#define SPECPRGMGR      4
#define SPECTLBMGR      5
#define SPECSYSMGR      6
#define GETCPUTIME      7
#define WAITFORCLOCK    8
#define WAITFORIO       9

/* specific message values */
#define ANYMESSAGE      0
#define TRAPCONTINUE    1
#define TRAPTERMINATE   0

#define CREATEGOOD      0       /* For thread creation success */
#define MSGGOOD         0       /* For message send success */
#define CREATENOGOOD    -1      /* For thread creation failure */
#define MSGNOGOOD       -1      /* For message send failure */

/****************************************************************************/

/* device register field number for non-terminal devices */
#define STATUS          0
#define COMMAND         1
#define DATA0           2
#define DATA1           3

/* device register field number for terminal devices */
#define RECVSTATUS      0
#define RECVCOMMAND     1
#define TRANSTATUS      2
#define TRANCOMMAND     3

/* device common STATUS codes */
#define UNINSTALLED     0
#define READY           1
#define BUSY            3

/* device common COMMAND codes */
#define RESET           0
#define ACK             1

#define STATUSMASK      0x000000FF
#define CAUSEMASK       0x000000FF

/* terminal specific address and codes */
#define TERMINAL0ADDR   0x10000250
#define TERMSTATUSMASK  0x000000FF
#define PRINTCHAR       2
#define CHAROFFSET      8
#define TRANSMITTED     5
#define RECEIVED        5

/* Useful operations */
#define STCK(T) ((T) = GET_TODLOW)

#define MsgSend(code,dest,payload) (SYSCALL((unsigned int) (code),(unsigned int) (dest),(unsigned int) (payload),0))

#define MsgRecv(code,source,reply) (((tcb_t *) SYSCALL((unsigned int) (code),(unsigned int) (source),(unsigned int) (reply),0)))

#endif
