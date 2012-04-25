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

	Questo modulo implementa alcune funzionalità utili per la gestione 
	dei Trap Manager.

************************************************************************/


#include <boot.e>

/* Array utile al nucleo per gestire la presenza dei trap manager */
tcb_t *trap_managers[MAXTHREADS];

/* Aggiunge un puntatore al manager tcb nel primo posto libero */
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

/* Rimuove dall'array (se c'è) il manager indicato */
void delete_manager(tcb_t *manager){
	int i;
	for (i=0;i<MAXTHREADS;i++)
		if (trap_managers[i]==manager) {
			trap_managers[i]=NULL;
			break;
		}
}

/* Verifica la presenza del manager nell'array nel qual caso ne restituisce il puntatore, altrimenti NULL */
tcb_t *thereIs_manager(tcb_t *manager){
	int i;
	for (i=0;i<MAXTHREADS;i++)
		if (trap_managers[i]==manager) return manager;
	return NULL;
}
