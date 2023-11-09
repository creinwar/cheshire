#ifndef THREAD_H_
#define THREAD_H_

#include "rv64.h"

typedef struct thread {
    unsigned long int tid;
    unsigned long int *stack;
    struct thread *next;
    sv39_pte_u *vspace;
    unsigned long int retval;
    unsigned long int exited;
    unsigned long int dtlb_misses;
} thread_t;

void thread_exit(unsigned long int retval);
int thread_create(thread_t *t, void *entry, void *sp,
                    unsigned long int sstatus, sv39_pte_u *vspace_root,
                    unsigned long int colours, unsigned long int arg0, unsigned long int arg1);
void thread_set_id(thread_t *t, unsigned long int asid, unsigned long int vmid);
void thread_get_id(thread_t *t, unsigned long int *asid, unsigned long int *vmid);
int thread_enqueue(thread_t *q, thread_t *t);
int thread_dequeue(thread_t *q, unsigned int tid);
thread_t *thread_schedule();
void thread_join(thread_t *t);
void thread_yield(void);
void thread_kickoff(thread_t *q);
void thread_set_timer(void);
void thread_trace(void);

#endif