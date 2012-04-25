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


#ifndef BOOT_E
#define BOOT_E

#include <base.h>
#include <types11.h>
#include <listx.h>
#include <const.h>
#include <const11.h>


/* Magic number per riferirsi ad SSI */
#define MAGIC_SSI 0xFFFFFFFF

extern struct list_head ready_queue;

extern struct list_head wait_queue;

extern U32 thread_count;

extern U32 soft_block_count;

extern tcb_t *current_thread;

extern U32 pseudo_tick;

extern U32 start_pseudo_tick;

extern U32 current_thread_tod;

extern tcb_t *SSI_tcb;

extern void test();

extern tcb_t *TEST_tcb;

#endif
