#ifndef MICRO_VMM_C_
#define MICRO_VMM_C_

#include <stddef.h>

#include "dif/clint.h"
#include "micro_vmm.h"
#include "rv64.h"

#define ENABLE_HPMCOUNTER_TRACE 1

extern void rv64_vm_kickoff(void);

#define VM_GUEST_TICKLEN      5

vm_t *cur_vm = NULL;
vm_t *vm_queue = NULL;

static unsigned long int next_vmid = 1;

extern void *__base_regs;

void __attribute__((noinline)) vm_park(void)
{
    uint32_t *dtlb_misses = (uint32_t *) ((uint64_t) &__base_regs + (cur_vm->vmid + 2)*4);
    *dtlb_misses = (cur_vm->dtlb_misses << 1) | 1;
    cur_vm->exited = 1;

    while(1){
        /*__asm volatile(
            "wfi\n"
            :::
        );*/
    }
}

int vm_create(vm_t *vm, unsigned long int hstatus, unsigned long int vsstatus,
                    unsigned long int asid, unsigned long int vmid, unsigned long int colours,
                    sv39_pte_u *host_vspace, sv39_pte_u *guest_vspace,
                    void *entry, void *sp, unsigned long int arg0, unsigned long int arg1)
{
    if(!vm)
        return -1;

    saved_state_t *st = (saved_state_t *) (sp - sizeof(saved_state_t));

    for(int i = 0; i < sizeof(rv64_int_regs_u)/sizeof(unsigned long int); i++){
        st->regs.raw[i] = 0;
    }

    for(int i = 0; i < sizeof(rv64_fp_regs_u)/sizeof(unsigned long int); i++){
        st->fp_regs.raw[i] = 0;
    }

    for(int i = 0; i < sizeof(rv64_csrs_u)/sizeof(unsigned long int); i++){
        st->csrs.raw[i] = 0;
    }

    st->regs.x.pc  = (unsigned long int) entry;
    st->regs.x.x1  = (unsigned long int) vm_park;
    st->regs.x.x2  = (unsigned long int) sp;
    st->regs.x.x10 = arg0;
    st->regs.x.x11 = arg1;

    st->csrs.x.hstatus  = hstatus;
    st->csrs.x.hgatp    = host_vspace ?  8UL << 60 | (vmid & 0x3FFF) << 44 | ((unsigned long int) host_vspace)  >> 12 : 0;
    st->csrs.x.vsstatus = vsstatus;
    st->csrs.x.vsatp    = guest_vspace ? 8UL << 60 | (asid & 0xFFFF) << 44 | ((unsigned long int) guest_vspace) >> 12 : 0;
    st->csrs.x.cur_clrs = colours;

    vm->vmid            = next_vmid++;
    vm->sp              = (unsigned long int *) st;
    vm->next            = NULL;
    vm->guest_vspace    = guest_vspace;
    vm->host_vspace     = host_vspace;
    vm->exited          = 0;
    vm->dtlb_misses     = 0;

    return 0;
}

int vm_enqueue(vm_t *q, vm_t *vm)
{
    if(!q || !vm)
        return -1;

    vm_t *iter = NULL;
    // Make sure to only insert each vm once
    for(iter = q; iter->next; iter = iter->next){
        if(iter->vmid == vm->vmid)
            return 0;
    }

    vm->next = NULL;
    iter->next = vm;

    return 0;
}

int vm_dequeue(vm_t *q, unsigned int vmid)
{
    if(!q)
        return -1;

    vm_t *iter = NULL;
    for(iter = q; iter->next && iter->next->vmid != vmid; iter = iter->next){}

    // The vm to be dequeued was actually found
    if(iter->next && iter->next->vmid == vmid){
        // Dequeue the thread
        iter->next = iter->next->next;
        return 0;
    // No thread with the specified thread ID could be found
    } else {
        return -1;
    }
}

// Dumb round robin scheduler
// Returns either the next thread in the list or the root of the list
vm_t *vm_schedule()
{
    if(!vm_queue || !cur_vm)
        return cur_vm;

    vm_t *tmp = cur_vm;

    // If the current thread has exited, remove it from the queue
    if(cur_vm->exited){
        // Easy case, the queue root exited
        if(vm_queue == cur_vm){
            vm_queue = cur_vm->next;
        } else {
            for(tmp = vm_queue; tmp->next && tmp->next != cur_vm; tmp = tmp->next){}

            if(tmp->next && tmp->next == cur_vm){
                tmp->next = cur_vm->next;
            } // else what? We did not find the thread that exited in the queue??
        }
    }

    cur_vm = (tmp->next) ? tmp->next : vm_queue;

    return cur_vm;
}

void vm_kickoff(vm_t *q)
{
    if(!q)
        return;

    cur_vm = q;
    vm_queue = q;

    rv64_vm_kickoff();
}

void vm_set_timer(void)
{
    clint_set_mtimecmpx(0, clint_get_mtime() + VM_GUEST_TICKLEN);
    return;
}

void vm_trace(void)
{
#ifdef ENABLE_HPMCOUNTER_TRACE
    unsigned long int incr = 0;
    __asm volatile(
        "li a0, 13\n                \
         ecall\n                    \
         add %0, x0, a0\n"
        : "=r"(incr)
        ::  "a0"
    );

    cur_vm->dtlb_misses += incr;
    return;
#endif
}


#endif