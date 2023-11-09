#include "thread.h"


char t1_stack[4096];
char t2_stack[4096];
char t3_stack[4096];

extern void *__base_regs;

volatile unsigned int * const scr1 = (unsigned int *) ((unsigned long int) &__base_regs + 0x4);
volatile unsigned int * const scr2 = (unsigned int *) ((unsigned long int) &__base_regs + 0x8);
volatile unsigned int * const scr3 = (unsigned int *) ((unsigned long int) &__base_regs + 0xC);
volatile unsigned int * const scr4 = (unsigned int *) ((unsigned long int) &__base_regs + 0x10);

void t1_thread_func(void)
{
    volatile unsigned long int *a = (unsigned long int *) 0x280000000UL;    // 0x80000000
    unsigned long int sum = 0;
    while(1){
        for(int i = 0; i < 3072; i++){
            a[i] = 420.69f + i;
        }

        for(int i = 0; i < 3072; i++){
            sum += a[i];
        }
        *scr1 = (unsigned int) sum;
    }
}

void t2_thread_func(void)
{
    volatile double *b = (double *) 0x3c0010000UL;    // 0x80010000
    double sum = 0;
    unsigned long int lock_vpn = 0, lock_id = 1;
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

    lock_vpn = 0x3c0000000UL | 0x13UL;

    // Lock giga page TLB entry (although the page table says something else)
    __asm volatile(
        "csrrw x0, 0x5C1, %0\n      \
         csrrw x0, 0x5C2, %1\n      \
         csrrw x0, 0x5C3, %2\n"
        :: "r"(lock_pte), "r"(lock_vpn), "r"(lock_id)
        :
    );

    while(1){
        for(int i = 0; i < 16384; i++){
            //b[i] = 420.69f + i;
            b[i] = i;
        }

        for(int i = 0; i < 16384; i++){
            sum += b[i];
        }
        *scr1 = (unsigned int) sum;

        __asm volatile(
            "csrrwi x0, 0x5C3, 0\n      \
             csrrwi x0, 0x5C2, 0\n      \
             csrrwi x0, 0x5C1, 0\n"
            :::
        );
    }
}

void t3_thread_func(void)
{
    double sum = 0, start = (double) *scr3;

    sum = 0;
    for(int i = 0; i < 10000; i++){
        sum += start/8;
    }

    *scr4 = (unsigned int) sum;

    return;
}

// We only use the Gigapage tables
static sv39_pte_u __attribute__((aligned(4096))) vspace_t1[512];
static sv39_pte_u __attribute__((aligned(4096))) vspace_t2[512];
static sv39_pte_u __attribute__((aligned(4096))) vspace_t2_L2[512];
static sv39_pte_u __attribute__((aligned(4096))) vspace_t2_L3[512];
static sv39_pte_u __attribute__((aligned(4096))) vspace_t3[512];

int main(void)
{
    _Static_assert(sizeof(sv39_pte_u) == sizeof(unsigned long int), "PTE size missmatch");

    *scr3 = 1;

    thread_t t1, t2, t3;

    // First zero both vspaces
    for(int i = 0; i < 512; i++){
        vspace_t1[i].raw = 0UL;
        vspace_t2[i].raw = 0UL;
        vspace_t2_L2[i].raw = 0UL;
        vspace_t2_L3[i].raw = 0UL;
        vspace_t3[i].raw = 0UL;
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

    // Map different gigapages for t1 and t2 to DRAM
    vspace_t1[10].pte.ppn2 = 2;     // 0x280000000 -> 0x80000000
    vspace_t1[10].pte.v = 1;
    vspace_t1[10].pte.r = 1;
    vspace_t1[10].pte.w = 1;
    vspace_t1[10].pte.x = 0;
    vspace_t1[10].pte.a = 1;
    vspace_t1[10].pte.d = 1;

    vspace_t2[15].pte.ppn2 = ((unsigned long int) vspace_t2_L2) >> (12 + 9 + 9);
    vspace_t2[15].pte.ppn1 = ((unsigned long int) vspace_t2_L2) >> (12 + 9);
    vspace_t2[15].pte.ppn0 = ((unsigned long int) vspace_t2_L2) >> 12;
    vspace_t2[15].pte.v = 1;
    
    vspace_t2_L2[0].pte.ppn2 = ((unsigned long int) vspace_t2_L3) >> (12 + 9 + 9);
    vspace_t2_L2[0].pte.ppn1 = ((unsigned long int) vspace_t2_L3) >> (12 + 9);
    vspace_t2_L2[0].pte.ppn0 = ((unsigned long int) vspace_t2_L3) >> 12;
    vspace_t2_L2[0].pte.v = 1;


    for(int i = 16; i < 48; i++){
        vspace_t2_L3[i].pte.ppn2 = 2;   // 0x3c0000000 -> 0x80000000
        vspace_t2_L3[i].pte.ppn0 = i;
        vspace_t2_L3[i].pte.r = 1;
        vspace_t2_L3[i].pte.w = 1;
        vspace_t2_L3[i].pte.x = 0;
        vspace_t2_L3[i].pte.a = 1;
        vspace_t2_L3[i].pte.d = 1;
        vspace_t2_L3[i].pte.v = 1;
    }

    thread_create(&t1, (void *) t1_thread_func, (void *) (t1_stack + 4096), 2UL << 13, vspace_t1, 0x6000UL, 0, 0);
    thread_create(&t2, (void *) t2_thread_func, (void *) (t2_stack + 4096), 2UL << 13, vspace_t2, 0x100FUL, 0, 0);
    thread_create(&t3, (void *) t3_thread_func, (void *) (t3_stack + 4096), 2UL << 13, vspace_t3, 0x8000UL, 0, 0);

    thread_enqueue(&t1, &t2);
    thread_enqueue(&t1, &t3);

    thread_kickoff(&t1);

    while(1){
    }
}
