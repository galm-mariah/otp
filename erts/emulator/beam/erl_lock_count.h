/*
 * %CopyrightBegin%
 *
 * Copyright Ericsson AB 2008-2010. All Rights Reserved.
 *
 * The contents of this file are subject to the Erlang Public License,
 * Version 1.1, (the "License"); you may not use this file except in
 * compliance with the License. You should have received a copy of the
 * Erlang Public License along with this software. If not, it can be
 * retrieved online at http://www.erlang.org/.
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * %CopyrightEnd%
 */

/*
 * Description:	Statistics for locks.
 *
 * Author:	Bj�rn-Egil Dahlberg
 * Date:	2008-07-03
 * Abstract:
 * 	Locks statistics internal representation.
 * 	
 * 	Conceptual representation, 
 * 	- set name
 * 	| - id (the unique lock)
 * 	| | - lock type
 * 	| | - statistics
 * 	| | | - location (file and line number)
 * 	| | | - tries
 * 	| | | - collisions (including trylock busy)
 * 	| | | - timer (time spent in waiting for lock)
 * 	| | | - n_timer (collisions excluding trylock busy)
 *
 * 	Each instance of a lock is the unique lock, i.e. set and id in that set.
 * 	For each lock there is a set of statistics with where and what impact
 * 	the lock aqusition had.
 *
 * 	Runtime options
 * 	- suspend, used when internal lock-counting can't be applied. For instance
 * 	  when allocating a term for the outside and halloc needs to be used.
 * 	  Default: off.
 * 	- location, reserved and not used.
 * 	- proclock, disable proclock counting. Used when performance might be an
 * 	  issue. Accessible from erts_debug:lock_counters({process_locks, bool()}).
 * 	  Default: off.
 * 	- copysave, enable saving of destroyed locks (and thereby its statistics).
 * 	  If memory constraints is an issue this need to be disabled.
 * 	  Accessible from erts_debug:lock_counters({copy_save, bool()}).
 * 	  Default: off.
 *
 */

#include "sys.h"

#ifndef ERTS_LOCK_COUNT_H__
#define ERTS_LOCK_COUNT_H__

#ifdef  ERTS_ENABLE_LOCK_COUNT
#include "ethread.h"

#define ERTS_LCNT_MAX_LOCK_LOCATIONS (10)

#define ERTS_LCNT_LT_SPINLOCK   (((Uint16) 1) << 0)
#define ERTS_LCNT_LT_RWSPINLOCK (((Uint16) 1) << 1)
#define ERTS_LCNT_LT_MUTEX      (((Uint16) 1) << 2)
#define ERTS_LCNT_LT_RWMUTEX    (((Uint16) 1) << 3)
#define ERTS_LCNT_LT_PROCLOCK   (((Uint16) 1) << 4)
#define ERTS_LCNT_LT_ALLOC      (((Uint16) 1) << 5)

#define ERTS_LCNT_LO_READ       (((Uint16) 1) << 6)
#define ERTS_LCNT_LO_WRITE      (((Uint16) 1) << 7)

#define ERTS_LCNT_LO_READ_WRITE ( ERTS_LCNT_LO_READ  \
				| ERTS_LCNT_LO_WRITE )

#define ERTS_LCNT_LT_ALL        ( ERTS_LCNT_LT_SPINLOCK   \
				| ERTS_LCNT_LT_RWSPINLOCK \
				| ERTS_LCNT_LT_MUTEX      \
				| ERTS_LCNT_LT_RWMUTEX    \
				| ERTS_LCNT_LT_PROCLOCK   )
/* runtime options */

#define ERTS_LCNT_OPT_SUSPEND   (((Uint16) 1) << 0)
#define ERTS_LCNT_OPT_LOCATION  (((Uint16) 1) << 1)
#define ERTS_LCNT_OPT_PROCLOCK  (((Uint16) 1) << 2)
#define ERTS_LCNT_OPT_COPYSAVE  (((Uint16) 1) << 3)

typedef struct {
    unsigned long s;
    unsigned long ns;
} erts_lcnt_time_t;
    
extern erts_lcnt_time_t timer_start;

typedef struct erts_lcnt_lock_stats_s {
    /* "tries" and "colls" needs to be atomic since
     * trylock busy does not aquire a lock and there
     * is no post action to rectify the situation
     */

    char         *file;       /* which file the lock was taken */
    unsigned int  line;       /* line number in file */
    
    ethr_atomic_t tries;      /* n tries to get lock */
    ethr_atomic_t colls;      /* n collisions of tries to get lock */
    
    unsigned long timer_n;    /* #times waited for lock */
    erts_lcnt_time_t timer;   /* total wait time for lock */
} erts_lcnt_lock_stats_t;

/* rw locks uses both states, other locks only uses w_state */
typedef struct erts_lcnt_lock_s {
    char *name;            /* lock name */
    Uint16 flag;           /* lock type */
    Eterm id;              /* id if possible */ 

#ifdef DEBUG
    ethr_atomic_t flowstate;
#endif

    /* lock states */    
    ethr_atomic_t w_state; /* 0 not taken, otherwise n threads waiting */
    ethr_atomic_t r_state; /* 0 not taken, > 0 -> writes will wait */

    /* statistics */
    unsigned int n_stats;
    erts_lcnt_lock_stats_t stats[ERTS_LCNT_MAX_LOCK_LOCATIONS]; /* first entry is "undefined"*/
    
    /* chains for list handling  */
    /* data is hold by lcnt_lock */
    struct erts_lcnt_lock_s *prev;
    struct erts_lcnt_lock_s *next;
} erts_lcnt_lock_t;

typedef struct {
    erts_lcnt_lock_t *head;
    erts_lcnt_lock_t *tail;
    unsigned long n;
} erts_lcnt_lock_list_t;
    
typedef struct {
    erts_lcnt_time_t duration;			/* time since last clear */
    erts_lcnt_lock_list_t *current_locks;
    erts_lcnt_lock_list_t *deleted_locks;
} erts_lcnt_data_t;

typedef struct {
    int id;

    erts_lcnt_time_t timer;	/* timer         */
    int timer_set;		/* bool          */
    int lock_in_conflict;       /* bool          */
    
    /* function pointer */
    void *(*function)(void *);
    void *argument;
    
} erts_lcnt_thread_data_t;

/* globals */

extern Uint16 erts_lcnt_rt_options;

/* function declerations */

void erts_lcnt_init(void);

/* list operations (local)  */
erts_lcnt_lock_list_t *erts_lcnt_list_init(void);

void erts_lcnt_list_clear( erts_lcnt_lock_list_t *list);
void erts_lcnt_list_insert(erts_lcnt_lock_list_t *list, erts_lcnt_lock_t *lock);
void erts_lcnt_list_delete(erts_lcnt_lock_list_t *list, erts_lcnt_lock_t *lock);

/* lock operations (global) */
void erts_lcnt_init_lock(erts_lcnt_lock_t *lock, char *name, Uint16 flag);
void erts_lcnt_init_lock_x(erts_lcnt_lock_t *lock, char *name, Uint16 flag, Eterm id);
void erts_lcnt_destroy_lock(erts_lcnt_lock_t *lock);

void erts_lcnt_lock(erts_lcnt_lock_t *lock);
void erts_lcnt_lock_opt(erts_lcnt_lock_t *lock, Uint16 option);
void erts_lcnt_lock_post(erts_lcnt_lock_t *lock);
void erts_lcnt_lock_post_x(erts_lcnt_lock_t *lock, char *file, unsigned int line);
void erts_lcnt_lock_unaquire(erts_lcnt_lock_t *lock);

void erts_lcnt_unlock(erts_lcnt_lock_t *lock);
void erts_lcnt_unlock_opt(erts_lcnt_lock_t *lock, Uint16 option);

void erts_lcnt_trylock_opt(erts_lcnt_lock_t *lock, int res, Uint16 option);
void erts_lcnt_trylock(erts_lcnt_lock_t *lock, int res);

/* thread operations */

int erts_lcnt_thr_create(ethr_tid *tid, void * (*function)(void *), void *arg, ethr_thr_opts *opts);

/* bif interface */

Uint16 erts_lcnt_set_rt_opt(Uint16 opt);
Uint16 erts_lcnt_clear_rt_opt(Uint16 opt);
void   erts_lcnt_clear_counters(void);
char  *erts_lcnt_lock_type(Uint16 type);
erts_lcnt_data_t *erts_lcnt_get_data(void);

#endif /* ifdef  ERTS_ENABLE_LOCK_COUNT  */
#endif /* ifndef ERTS_LOCK_COUNT_H__     */