#ifndef TCB_E
#define TCB_E

#include <const.h>
#include <types11.h>
#include <listx.h>


void init_t_state (state_t *state);

/* TCB handling functions */

void initTcbs(void);

void freeTcb(tcb_t *p);

tcb_t *allocTcb(void);

void mkEmptyThreadQ(struct list_head *emptylist);

int emptyThreadQ(struct list_head *head);

void insertThread(struct list_head *head, tcb_t *p);

tcb_t *removeThread(struct list_head *head);

tcb_t *outThread(struct list_head *head, tcb_t *p);

tcb_t *headThread(struct list_head *head);


/* Tree view functions */

int emptyChild(tcb_t *this);

void insertChild(tcb_t *parent, tcb_t *child);

tcb_t *removeChild(tcb_t *parent);

tcb_t *outChild(tcb_t *child);

/**** Utili per phase 2 ****/

tcb_t *thereIsThread(struct list_head *head, tcb_t *p);

void insertSibling(tcb_t *sibling, tcb_t *new);

#endif
