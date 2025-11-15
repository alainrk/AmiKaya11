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

/**************************** MANAGER.C ********************************

	This module implements some functionalities useful for managing
	Trap Managers.

************************************************************************/


#include <boot.e>

/* Array useful for the kernel to manage the presence of trap managers */
tcb_t *trap_managers[MAXTHREADS];

/* Adds a pointer to the manager tcb in the first free position */
void add_manager(tcb_t *manager){
	int i;
	for (i=0;i<MAXTHREADS;i++){
		if (trap_managers[i]==manager) break;
		if (trap_managers[i]==NULL) {
			trap_managers[i]=manager;
			break;
		}
	}
}

/* Removes from the array (if it exists) the indicated manager */
void delete_manager(tcb_t *manager){
	int i;
	for (i=0;i<MAXTHREADS;i++)
		if (trap_managers[i]==manager) {
			trap_managers[i]=NULL;
			break;
		}
}

/* Checks the presence of the manager in the array, in which case returns its pointer, otherwise NULL */
tcb_t *thereIs_manager(tcb_t *manager){
	int i;
	for (i=0;i<MAXTHREADS;i++)
		if (trap_managers[i]==manager) return manager;
	return NULL;
}
