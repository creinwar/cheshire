#ifndef MICRO_VMM_H_
#define MICRO_VMM_H_

#include "rv64.h"

typedef struct vm_guest {
    unsigned long int vmid;
    unsigned long int *sp;
    struct vm_guest *next;
    sv39_pte_u *guest_vspace;
    sv39_pte_u *host_vspace;
    unsigned long int exited;
    unsigned long int dtlb_misses;
} vm_t;


int vm_create(vm_t *vm, unsigned long int hstatus, unsigned long int vsstatus,
                    unsigned long int asid, unsigned long int vmid, unsigned long int colours,
                    sv39_pte_u *host_vspace, sv39_pte_u *guest_vspace,
                    void *entry, void *sp, unsigned long int arg0, unsigned long int arg1);
int vm_enqueue(vm_t *q, vm_t *t);
int vm_dequeue(vm_t *q, unsigned int vmid);

void vm_kickoff(vm_t *q);
void vm_set_timer(void);
void vm_trace(void);

#endif