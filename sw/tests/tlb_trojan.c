#include "thread.h"

#include "regs/cheshire.h"
#include "dif/clint.h"
#include "dif/uart.h"
#include "params.h"
#include "util.h"
#include "printf.h"

#define PAGE_SIZE 4096
#define CONFIG_BENCH_DATA_POINTS 4096
#define TROJAN_TLB_PAGES 16
#define SPY_TLB_PAGES    16

char t1_stack[1024];
char t2_stack[1024];

extern void *__base_regs;

volatile unsigned int * const scr1 = (unsigned int *) ((unsigned long int) &__base_regs + 0x4);
volatile unsigned int * const scr2 = (unsigned int *) ((unsigned long int) &__base_regs + 0x8);
volatile unsigned int * const scr3 = (unsigned int *) ((unsigned long int) &__base_regs + 0xC);
volatile unsigned int * const scr4 = (unsigned int *) ((unsigned long int) &__base_regs + 0x10);

static sv39_pte_u __attribute__((aligned(4096))) vspace_t1_L2[512];
static sv39_pte_u __attribute__((aligned(4096))) vspace_t1_L1[512];
static sv39_pte_u __attribute__((aligned(4096))) vspace_t1_L0[512];
static sv39_pte_u __attribute__((aligned(4096))) vspace_t2_L2[512];
static sv39_pte_u __attribute__((aligned(4096))) vspace_t2_L1[512];
static sv39_pte_u __attribute__((aligned(4096))) vspace_t2_L0[512];

static inline uint64_t rdtime() {
  uint64_t rv;
  asm volatile ("csrrs %0, cycle, x0\n" : "=r"(rv) ::);
  return rv;
}

static void __attribute__((bare)) tlb_access(void *base, uint64_t num_pages, uint64_t descending)
{
    __asm volatile(
        "add a0, x0, %0\n                                                           \
        add a1, x0, %1\n                                                            \
        add a2, x0, %2\n                                                            \
        beq a1, x0, 2f /* Don't touch any pages? Sure! */\n                         \
        /* Default increment: one 4k page in positive direction */\n                \
        addi a3, x0, 1\n                                                            \
        slli a3, a3, 12\n                                                           \
        beq a2, x0, 1f  /* Ascending or do we have to update the base address? */\n \
        addi t0, a1, -1\n                                                           \
        slli t1, t0, 12\n                                                           \
        add a0, a0, t1\n                                                            \
        /* Increment: one 4k page in NEGATIVE direction */\n                        \
        addi a3, x0, -1\n                                                           \
        slli a3, a3, 12\n                                                           \
1:\n                                                                                \
        ld t2, 0(a0)  /* Access the page */\n                                       \
        addi a1, a1, -1\n                                                           \
        add a0, a0, a3\n                                                            \
        blt x0, a1, 1b\n                                                            \
2:\n                                                                                \
        ret\n"
        :: "r"(base), "r"(num_pages), "r"(descending)
        :);
}

int tlb_trojan(char *buf) { 
    uint32_t secret = 3;

    for (int i = 0; i < CONFIG_BENCH_DATA_POINTS; i++) {

        __asm volatile("fence\n" ::: "memory");

        if(secret == 6){
            secret = 4;
        } else {
            secret++;
        }

        tlb_access(buf, secret, 0);

        thread_yield();
    }

    return 0;
}

int tlb_spy(char *buf) {
    uint64_t probe_start = 0, probe_end = 0;
    uint32_t direction = 0;

    for (int i = 0; i < CONFIG_BENCH_DATA_POINTS; i++) {
        __asm volatile("fence" ::: "memory");

        probe_start = rdtime();

        tlb_access(buf, SPY_TLB_PAGES, direction);

        probe_end = rdtime();

        /*result is the total probing cost
          secret is updated by trojan in the previous system tick*/
        *scr3 = (uint32_t) (probe_end - probe_start);
        direction = !direction;
        thread_yield();
    }
    
    return 0;
}

thread_t t1, t2, t3;
int main(void)
{
    _Static_assert(sizeof(sv39_pte_u) == sizeof(unsigned long int), "PTE size missmatch");

    //uint32_t rtc_freq = *reg32(&__base_regs, CHESHIRE_RTC_FREQ_REG_OFFSET);
    //uint64_t reset_freq = clint_get_core_freq(rtc_freq, 2500);
    //uart_init(&__base_uart, reset_freq, 115200);

    //printf("---- TLB Trojan ----\r\n");
    //printf("[*] UART initialized\r\n");
    //printf("[*] RTC freq: %u, Core freq: %lu\r\n", rtc_freq, reset_freq);

    // First zero both vspaces
    for(int i = 0; i < 512; i++){
        vspace_t1_L2[i].raw = 0UL;
        vspace_t1_L1[i].raw = 0UL;
        vspace_t1_L0[i].raw = 0UL;
        vspace_t2_L2[i].raw = 0UL;
        vspace_t2_L1[i].raw = 0UL;
        vspace_t2_L0[i].raw = 0UL;
    }

    // Create address spaces for t1 and t2
    // Identity map 0x0 -> 0xFFFFFFFF (first 4 gigapages)
    // Also give full permissions
    for(int i = 0; i < 4; i++){
        vspace_t1_L2[i].pte.ppn2 = i;
        vspace_t1_L2[i].pte.v = 1;
        vspace_t1_L2[i].pte.r = 1;
        vspace_t1_L2[i].pte.w = 1;
        vspace_t1_L2[i].pte.x = 1;
        vspace_t1_L2[i].pte.a = 1;
        vspace_t1_L2[i].pte.d = 1;

        vspace_t2_L2[i].pte.ppn2 = i;
        vspace_t2_L2[i].pte.v = 1;
        vspace_t2_L2[i].pte.r = 1;
        vspace_t2_L2[i].pte.w = 1;
        vspace_t2_L2[i].pte.x = 1;
        vspace_t2_L2[i].pte.a = 1;
        vspace_t2_L2[i].pte.d = 1;
    }

    /// LVL 2 -> LVL 1 --------
    vspace_t1_L2[10].pte.ppn2 = ((unsigned long int) vspace_t1_L1) >> (12 + 9 + 9);
    vspace_t1_L2[10].pte.ppn1 = ((unsigned long int) vspace_t1_L1) >> (12 + 9);
    vspace_t1_L2[10].pte.ppn0 = ((unsigned long int) vspace_t1_L1) >> 12;
    vspace_t1_L2[10].pte.v = 1;

    vspace_t2_L2[10].pte.ppn2 = ((unsigned long int) vspace_t2_L1) >> (12 + 9 + 9);
    vspace_t2_L2[10].pte.ppn1 = ((unsigned long int) vspace_t2_L1) >> (12 + 9);
    vspace_t2_L2[10].pte.ppn0 = ((unsigned long int) vspace_t2_L1) >> 12;
    vspace_t2_L2[10].pte.v = 1;
    /// -----------------------


    /// LVL 1 -> LVL 0 --------
    vspace_t1_L1[0].pte.ppn2 = ((unsigned long int) vspace_t1_L0) >> (12 + 9 + 9);
    vspace_t1_L1[0].pte.ppn1 = ((unsigned long int) vspace_t1_L0) >> (12 + 9);
    vspace_t1_L1[0].pte.ppn0 = ((unsigned long int) vspace_t1_L0) >> 12;
    vspace_t1_L1[0].pte.v = 1;

    vspace_t2_L1[0].pte.ppn2 = ((unsigned long int) vspace_t2_L0) >> (12 + 9 + 9);
    vspace_t2_L1[0].pte.ppn1 = ((unsigned long int) vspace_t2_L0) >> (12 + 9);
    vspace_t2_L1[0].pte.ppn0 = ((unsigned long int) vspace_t2_L0) >> 12;
    vspace_t2_L1[0].pte.v = 1;
    /// -----------------------

    // Map the whole lowest 2M
    for(int i = 0; i < 512; i++){
        vspace_t1_L0[i].pte.ppn2 = 2;   // 0x280000000 -> 0x80000000
        vspace_t1_L0[i].pte.ppn0 = i;
        vspace_t1_L0[i].pte.r = 1;
        vspace_t1_L0[i].pte.w = 1;
        vspace_t1_L0[i].pte.x = 0;
        vspace_t1_L0[i].pte.a = 1;
        vspace_t1_L0[i].pte.d = 1;
        vspace_t1_L0[i].pte.v = 1;

        vspace_t2_L0[i].pte.ppn2 = 2;   // 0x280000000 -> 0x80000000
        vspace_t2_L0[i].pte.ppn0 = i;
        vspace_t2_L0[i].pte.r = 1;
        vspace_t2_L0[i].pte.w = 1;
        vspace_t2_L0[i].pte.x = 0;
        vspace_t2_L0[i].pte.a = 1;
        vspace_t2_L0[i].pte.d = 1;
        vspace_t2_L0[i].pte.v = 1;
    }

    //printf("[*] Page tables initialized\r\n");

    thread_create(&t1, (void *) tlb_trojan, (void *) (t1_stack + 1024), 2UL << 13, vspace_t1_L2, 0xFFFFUL, 0x280000000UL, 0);
    thread_set_id(&t1, 0xAAAAUL, 0);

    thread_create(&t2, (void *) tlb_spy, (void *) (t2_stack + 1024), 2UL << 13, vspace_t2_L2, 0xFFFFUL, 0x280000000UL, 0);
    thread_set_id(&t2, 0xBBBBUL, 0);

    //printf("[*] Threads created\r\n");

    /*unsigned long int lock_vpn = 0, lock_id = (0xBBBBUL << 15) | 1;
    sv39_pte_u lock_pte = {0};

    lock_pte.pte.ppn2 = 2;   // 0x280000000 -> 0x80000000
    lock_pte.pte.ppn1 = 0;
    lock_pte.pte.ppn0 = 0;
    lock_pte.pte.r = 1;
    lock_pte.pte.w = 1;
    lock_pte.pte.x = 0;
    lock_pte.pte.a = 1;
    lock_pte.pte.d = 1;
    lock_pte.pte.v = 1;

    // 2M page, non-virtual, data and s-stage enabled
    lock_vpn = 0x280000000UL | 0x32UL;

    // Lock giga page TLB entry (although the page table says something else)
    __asm volatile(
        "csrrw x0, 0x5C3, %0\n      \
         csrrw x0, 0x5C4, %1\n      \
         csrrw x0, 0x5C5, %2\n"
        :: "r"(lock_pte), "r"(lock_vpn), "r"(lock_id)
        :
    );

    //printf("[*] PTE locked\r\n");*/

    thread_enqueue(&t1, &t2);

    //printf("[*] Kicking off scheduler...\r\n");

    thread_kickoff(&t1);

    while(1){
    }
}
