#ifndef THREAD_C_
#define THREAD_C_

#include <stddef.h>

#include "dif/clint.h"
#include "thread.h"
#include "rv64.h"
#include "printf.h"

#define ENABLE_HPMCOUNTER_TRACE 1
//#define ENABLE_TRACE_PRINT 1

extern void rv64_thread_kickoff(void);

// Sim
#define THREAD_TICKLEN      5

// FPGA
//#define THREAD_TICKLEN        5*(31*4)  // The FPGA clint ticks 31 times as fast
                                        // and the core clock is 4x slower
thread_t *cur_thread = NULL;
thread_t *queue = NULL;

static unsigned long int next_tid = 1;

// Saves the return value and marks the thread as finished
void thread_exit(unsigned long int retval)
{
    cur_thread->retval = retval;
    cur_thread->exited = 1;

    thread_yield();

    while(1){
        asm volatile("wfi\n" ::: "memory");
    }
}

int thread_create(thread_t *t, void *entry, void *sp,
                    unsigned long int sstatus, sv39_pte_u *vspace_root,
                    unsigned long int colours, unsigned long int arg0, unsigned long int arg1)
{
    if(!t)
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

    // The new threads inherit our current global pointer
    uint64_t gp = 0;
    __asm volatile(
        "addi %0, gp, 0\n"
        : "=r"(gp)
        ::
    );

    st->regs.x.pc  = (unsigned long int) entry;
    st->regs.x.x1  = (unsigned long int) thread_exit;
    st->regs.x.x2  = (unsigned long int) sp;
    st->regs.x.x3  = gp;
    st->regs.x.x10 = arg0;
    st->regs.x.x11 = arg1;

    st->csrs.x.sstatus  = sstatus;
    // If vspace_root is not NULL, configure SV39
    st->csrs.x.satp     = vspace_root ? 8UL << 60 | ((unsigned long int) vspace_root) >> 12 : 0;
    st->csrs.x.cur_clrs = colours;
    
    t->next         = NULL;
    t->tid          = next_tid++;
    t->stack        = (unsigned long int *) st;
    t->vspace       = vspace_root;
    t->retval       = 0xDEADBEEF;
    t->exited       = 0;
    t->dtlb_misses  = 0;

    return 0;
}

void thread_set_id(thread_t *t, unsigned long int asid, unsigned long int vmid)
{
    if(!t)
        return;

    saved_state_t *st = (saved_state_t *) t->stack;

    st->csrs.x.satp = (st->csrs.x.satp & ~((unsigned long int) (0xFFFFUL << 44))) | (asid << 44);
}

void thread_get_id(thread_t *t, unsigned long int *asid, unsigned long int *vmid)
{

    if(!t || (!asid && !vmid))
        return;
    
    saved_state_t *st = (saved_state_t *) t->stack;

    if(asid)
        *asid = (unsigned long int) ((st->csrs.x.satp >> 44) & 0xFFFF);

    // VMID - TODO

}

int thread_enqueue(thread_t *q, thread_t *t)
{
    if(!q || !t)
        return -1;

    thread_t *iter = NULL;
    for(iter = q; iter->next; iter = iter->next){
        if(iter->tid == t->tid)
            return 0;
    }

    t->next = NULL;
    iter->next = t;

    return 0;
}

int thread_dequeue(thread_t *q, unsigned int tid)
{
    if(!q)
        return -1;

    thread_t *iter = NULL;
    for(iter = q; iter->next && iter->next->tid != tid; iter = iter->next){}

    // The thread to be dequeued was actually found
    if(iter->next && iter->next->tid == tid){
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
thread_t *thread_schedule()
{
    if(!queue || !cur_thread)
        return cur_thread;


    thread_t *tmp = cur_thread;

    // If the current thread has exited, remove it from the queue
    if(cur_thread->exited){
        // Easy case, the queue root exited
        if(queue == cur_thread){
            queue = cur_thread->next;
        } else {
            for(tmp = queue; tmp->next && tmp->next != cur_thread; tmp = tmp->next){}

            if(tmp->next && tmp->next == cur_thread){
                tmp->next = cur_thread->next;
            } // else what? We did not find the thread that exited in the queue??
        }
    }

    cur_thread = (tmp->next) ? tmp->next : queue;

    return cur_thread;
}

void thread_join(thread_t *t)
{
    if(!t)
        return;

    while(!t->exited){
        thread_yield();
    }
}

void __attribute__((noinline)) thread_yield(void)
{
    __asm volatile (
        "addi a0, x0, 69\n  \
         ecall\n            \
        "
        ::: "a0", "memory"
    );
}

void thread_kickoff(thread_t *q)
{
    if(!q)
        return;

    cur_thread = q;
    queue = q;

    rv64_thread_kickoff();
}

void thread_set_timer(void)
{
    clint_set_mtimecmpx(0, clint_get_mtime() + THREAD_TICKLEN);
    return;
}

void thread_dump_mstate(void)
{
    uint64_t mcause, mepc, mip, mie, mstatus, mtval;
    asm volatile("csrr %0, mcause; csrr %1, mepc; csrr %2, mip;"
                 "csrr %3, mie; csrr %4, mstatus; csrr %5, mtval"
                 : "=r"(mcause), "=r"(mepc), "=r"(mip), "=r"(mie), "=r"(mstatus), "=r"(mtval));
    printf("\r\n==== ctxt switch trace ====\r\n"
           " mcause:     0x%016x\r\n mepc:       0x%016x\r\n mip:        0x%016x\r\n"
           " mie:        0x%016x\r\n mstatus:    0x%016x\r\n mtval:      0x%016x\r\n"
           "================================\r\n",
           mcause, mepc, mip, mie, mstatus, mtval);
}


void thread_trace(void)
{
#ifdef ENABLE_HPMCOUNTER_TRACE
    unsigned long int incr = 0;
    __asm volatile(
        "csrrw %0, mhpmcounter3, x0\n"
        : "=r"(incr)
        ::
    );

    cur_thread->dtlb_misses += incr;
#endif

#ifdef ENABLE_TRACE_PRINT
    thread_dump_mstate();
#endif

    /*uint64_t data_ptes[16], instr_ptes[16];
    uint64_t data_flags[16], instr_flags[16];

    for(int i = 0; i < 16; i++){
        uint64_t cur_data_pte = 0, cur_instr_pte = 0;
        uint64_t cur_data_flags = 0, cur_instr_flags = 0;

        __asm volatile(
            "csrrw x0, 0x801, %4\n      \
             csrrw x0, 0x802, %4\n      \
             csrrs %0, 0x803, x0\n      \
             csrrs %1, 0x804, x0\n      \
             csrrs %2, 0x805, x0\n      \
             csrrs %3, 0x806, x0\n"
             : "=r"(cur_data_pte), "=r"(cur_instr_pte), "=r"(cur_data_flags), "=r"(cur_instr_flags)
             : "r"(i)
             :
        );
        data_ptes[i]    = cur_data_pte;
        data_flags[i]   = cur_data_flags;
        instr_ptes[i]   = cur_instr_pte;
        instr_flags[i]  = cur_instr_flags;
    }

    for(int i = 0; i < 16; i++){
        (void) data_flags[i];
        (void) data_ptes[i];
        (void) instr_flags[i];
        (void) instr_ptes[i];
    }

    //for(int i = 0; i < 16; i++){
    //    printf("Data TLB entry %d: pte = 0x%016lx, flags = 0x%016lx\r\n", i, data_ptes[i], data_flags[i]);
    //}

    //for(int i = 0; i < 16; i++){
    //    printf("Instr TLB entry %d: pte = 0x%016lx, flags = 0x%016lx\r\n", i, instr_ptes[i], instr_flags[i]);
    //}
    */

    return;
}

#endif
