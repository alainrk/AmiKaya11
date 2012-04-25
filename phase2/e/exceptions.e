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


#ifndef EXCEPTIONS_E
#define EXCEPTIONS_E

#include <base.h>
#include <types11.h>
#include <listx.h>
#include <const.h>

extern void save_state(state_t *source, state_t *dest);

extern void terminate (tcb_t *subtree_root);

extern int send (tcb_t *sender, tcb_t *target, U32 payload);

extern tcb_t *recv (tcb_t *receiver, tcb_t *sender, U32 *reply);

extern void sysbp_handler();
extern void pgmtrap_handler();
extern void tlb_handler();

#endif
