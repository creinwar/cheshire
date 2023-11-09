#include "thread.h"

#include "regs/cheshire.h"
#include "dif/clint.h"
#include "dif/uart.h"
#include "params.h"
#include "util.h"
#include "printf.h"

char t1_stack[1024];
char t2_stack[1024];
char t3_stack[1024];

extern void *__base_regs;

volatile unsigned int * const scr1 = (unsigned int *) ((unsigned long int) &__base_regs + 0x4);
volatile unsigned int * const scr2 = (unsigned int *) ((unsigned long int) &__base_regs + 0x8);
volatile unsigned int * const scr3 = (unsigned int *) ((unsigned long int) &__base_regs + 0xC);
volatile unsigned int * const scr4 = (unsigned int *) ((unsigned long int) &__base_regs + 0x10);

static sv39_pte_u __attribute__((aligned(4096))) vspace_t1[512];
static sv39_pte_u __attribute__((aligned(4096))) vspace_t1_L2[512];
static sv39_pte_u __attribute__((aligned(4096))) vspace_t1_L3[512];
static sv39_pte_u __attribute__((aligned(4096))) vspace_t2[512];
static sv39_pte_u __attribute__((aligned(4096))) vspace_t2_L2[512];
static sv39_pte_u __attribute__((aligned(4096))) vspace_t2_L3[512];
static sv39_pte_u __attribute__((aligned(4096))) vspace_t3[512];

void page_toucher(unsigned long int *base, unsigned long int num_pages)
{
    for(int iter = 0; iter < 1000; iter++){
        for(unsigned long int i = 0; i < num_pages; i++){
            base[i*512] = (unsigned long int) base;
        }
    }
}

void wait_two_tasks(thread_t *t1, thread_t *t2)
{
    char t1_done = 0, t2_done = 0;
    while(1){
        if(t1 && t1->exited && !t1_done){
            t1_done = 1;
            printf("First thread DTLB misses: %lu\n", t1->dtlb_misses);
        }

        if(t2 && t2->exited && !t2_done){
            t2_done = 1;
            printf("Second thread DTLB misses: %lu\n", t2->dtlb_misses);
        }

        thread_yield();
    }
}

thread_t t1, t2, t3;

int main(void)
{
    _Static_assert(sizeof(sv39_pte_u) == sizeof(unsigned long int), "PTE size missmatch");

    uint32_t rtc_freq = *reg32(&__base_regs, CHESHIRE_RTC_FREQ_REG_OFFSET);
    uint64_t reset_freq = clint_get_core_freq(rtc_freq, 2500);
    uart_init(&__base_uart, reset_freq, 115200);

    // First zero both vspaces
    for(int i = 0; i < 512; i++){
        vspace_t1[i].raw    = 0UL;
        vspace_t1_L2[i].raw = 0UL;
        vspace_t1_L3[i].raw = 0UL;
        vspace_t2[i].raw    = 0UL;
        vspace_t2_L2[i].raw = 0UL;
        vspace_t2_L3[i].raw = 0UL;
        vspace_t3[i].raw    = 0UL;
    }

    // Create address spaces for t1 and t2
    // Identity map 0x0 -> 0xFFFFFFFF (first 4 gigapages)
    // Also give full permissions
    for(int i = 0; i < 4; i++){
        vspace_t1[i].pte.ppn2 = i;
        vspace_t1[i].pte.v = 1;
        vspace_t1[i].pte.r = 1;
        vspace_t1[i].pte.w = 1;
        vspace_t1[i].pte.x = 1;
        vspace_t1[i].pte.a = 1;
        vspace_t1[i].pte.d = 1;

        vspace_t2[i].pte.ppn2 = i;
        vspace_t2[i].pte.v = 1;
        vspace_t2[i].pte.r = 1;
        vspace_t2[i].pte.w = 1;
        vspace_t2[i].pte.x = 1;
        vspace_t2[i].pte.a = 1;
        vspace_t2[i].pte.d = 1;

        vspace_t3[i].pte.ppn2 = i;
        vspace_t3[i].pte.v = 1;
        vspace_t3[i].pte.r = 1;
        vspace_t3[i].pte.w = 1;
        vspace_t3[i].pte.x = 1;
        vspace_t3[i].pte.a = 1;
        vspace_t3[i].pte.d = 1;
    }

    /// LVL 2 -> LVL 1 --------
    vspace_t1[10].pte.ppn2 = ((unsigned long int) vspace_t1_L2) >> (12 + 9 + 9);
    vspace_t1[10].pte.ppn1 = ((unsigned long int) vspace_t1_L2) >> (12 + 9);
    vspace_t1[10].pte.ppn0 = ((unsigned long int) vspace_t1_L2) >> 12;
    vspace_t1[10].pte.v = 1;

    vspace_t2[15].pte.ppn2 = ((unsigned long int) vspace_t2_L2) >> (12 + 9 + 9);
    vspace_t2[15].pte.ppn1 = ((unsigned long int) vspace_t2_L2) >> (12 + 9);
    vspace_t2[15].pte.ppn0 = ((unsigned long int) vspace_t2_L2) >> 12;
    vspace_t2[15].pte.v = 1;
    /// -----------------------


    /// LVL 1 -> LVL 0 --------
    vspace_t1_L2[0].pte.ppn2 = ((unsigned long int) vspace_t1_L3) >> (12 + 9 + 9);
    vspace_t1_L2[0].pte.ppn1 = ((unsigned long int) vspace_t1_L3) >> (12 + 9);
    vspace_t1_L2[0].pte.ppn0 = ((unsigned long int) vspace_t1_L3) >> 12;
    vspace_t1_L2[0].pte.v = 1;

    //vspace_t2_L2[0].pte.ppn2 = ((unsigned long int) vspace_t2_L3) >> (12 + 9 + 9);
    //vspace_t2_L2[0].pte.ppn1 = ((unsigned long int) vspace_t2_L3) >> (12 + 9);
    //vspace_t2_L2[0].pte.ppn0 = ((unsigned long int) vspace_t2_L3) >> 12;
    //vspace_t2_L2[0].pte.v = 1;

    // Instead, t2 terminates here (i.e. we have a 2M page)
    vspace_t2_L2[0].pte.ppn2 = 2;
    vspace_t2_L2[0].pte.ppn1 = 0;
    vspace_t2_L2[0].pte.ppn0 = 0;
    vspace_t2_L2[0].pte.r    = 1;
    vspace_t2_L2[0].pte.w    = 1;
    vspace_t2_L2[0].pte.x    = 0;
    vspace_t2_L2[0].pte.a    = 1;
    vspace_t2_L2[0].pte.d    = 1;
    vspace_t2_L2[0].pte.v    = 1;
    /// -----------------------


    // Map the whole 2M
    for(int i = 0; i < 512; i++){

        vspace_t1_L3[i].pte.ppn2 = 2;   // 0x280000000 -> 0x80000000
        vspace_t1_L3[i].pte.ppn0 = i;
        vspace_t1_L3[i].pte.r = 1;
        vspace_t1_L3[i].pte.w = 1;
        vspace_t1_L3[i].pte.x = 0;
        vspace_t1_L3[i].pte.a = 1;
        vspace_t1_L3[i].pte.d = 1;
        vspace_t1_L3[i].pte.v = 1;

        //vspace_t2_L3[i].pte.ppn2 = 2;   // 0x3c0000000 -> 0x80000000
        //vspace_t2_L3[i].pte.ppn0 = i;
        //vspace_t2_L3[i].pte.r = 1;
        //vspace_t2_L3[i].pte.w = 1;
        //vspace_t2_L3[i].pte.x = 0;
        //vspace_t2_L3[i].pte.a = 1;
        //vspace_t2_L3[i].pte.d = 1;
        //vspace_t2_L3[i].pte.v = 1;
    }

    // Task 1 gets the memory identity mapped for the code and a giga-page for data accesses
    thread_create(&t1, (void *) page_toucher, (void *) (t1_stack + 1024), 2UL << 13, vspace_t1, 0xFFFFUL, 0x280000000UL, 32);
    thread_set_id(&t1, 0xAAAAUL, 0);

    // Task 2 gets the same identity map, but 2M in 4k pages for data accesses
    thread_create(&t2, (void *) page_toucher, (void *) (t2_stack + 1024), 2UL << 13, vspace_t2, 0xFFFFUL, 0x3c0000000UL, 32);
    thread_set_id(&t2, 0xBBBBUL, 0);

    unsigned long int lock_vpn = 0, lock_id = (0xBBBBUL << 15) | 1;
    sv39_pte_u lock_pte = {0};

    lock_pte.pte.ppn2 = 2;   // 0x3c0000000 -> 0x80000000
    lock_pte.pte.ppn1 = 0;
    lock_pte.pte.ppn0 = 0;
    lock_pte.pte.r = 1;
    lock_pte.pte.w = 1;
    lock_pte.pte.x = 0;
    lock_pte.pte.a = 1;
    lock_pte.pte.d = 1;
    lock_pte.pte.v = 1;

    // 2M page, non-virtual, data and s-stage enabled
    lock_vpn = 0x3c0000000UL | 0x32UL;

    // Lock giga page TLB entry (although the page table says something else)
    __asm volatile(
        "csrrw x0, 0x5C1, %0\n      \
         csrrw x0, 0x5C2, %1\n      \
         csrrw x0, 0x5C3, %2\n"
        :: "r"(lock_pte), "r"(lock_vpn), "r"(lock_id)
        :
    );

    // Task 3 waits on task 1 and 2 and prints their DTLB misses
    thread_create(&t3, (void *) wait_two_tasks, (void *) (t3_stack + 1024), 2UL << 13, vspace_t3, 0xFFFEUL, (unsigned long int) &t1, (unsigned long int) &t2);

    // Both tasks
    thread_enqueue(&t1, &t2);
    thread_enqueue(&t2, &t3);

    // Only T1
    //thread_enqueue(&t1, &t3);

    thread_kickoff(&t1);

    while(1){
    }
}
