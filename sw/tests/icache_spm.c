#include "rv64.h"
#include "regs/cheshire.h"
#include "dif/clint.h"
#include "dif/uart.h"
#include "params.h"
#include "util.h"
#include "printf.h"

#define VM_OFFSET 0xFFFFFFC000000000ULL

extern void *__base_regs;

volatile unsigned int * const scr1 = (unsigned int *) ((unsigned long int) &__base_regs + 0x4);
volatile unsigned int * const scr2 = (unsigned int *) ((unsigned long int) &__base_regs + 0x8);
volatile unsigned int * const scr3 = (unsigned int *) ((unsigned long int) &__base_regs + 0xC);
volatile unsigned int * const scr4 = (unsigned int *) ((unsigned long int) &__base_regs + 0x10);

volatile static sv39_pte_u __attribute__((aligned(4096))) vspace_L2[512];
volatile static sv39_pte_u __attribute__((aligned(4096))) vspace_L1[512];
volatile static sv39_pte_u __attribute__((aligned(4096))) vspace_L0[512];

extern volatile void *_exit;

uint64_t __attribute__((noinline)) __attribute__((aligned(8))) spm_func(uint64_t a, uint64_t b)
{
    uint64_t tmp = 1;
    for(int i = 0; i < 100; i++){
        tmp = tmp*a*b;
    }
    return tmp;
}

void payload(void)
{
    volatile uint64_t *code = (volatile uint64_t *) (VM_OFFSET + 0x10000000);
    uint64_t sum = 0;

    volatile uint64_t *src = (volatile uint64_t *) (spm_func);
    volatile uint64_t *dst = (volatile uint64_t *) 0x01a00000;

    // Copy the payload function (128 bytes) to the icache spm
    for(int i = 0; i < 16; i++){
        *dst++ = *src++;
    }

    __asm volatile("fence.i\n" :::);

    uint64_t (*f)(uint64_t, uint64_t) = (uint64_t (*)(uint64_t, uint64_t)) 0x01a00000;

    dst--;
    sum = f(*src, *dst);

    for(int i = 0; i < 1000; i++){
        for(int j = 0; j < 128; j++){
            sum += code[j];
        }
    }

    *scr4 = (sum >> 32) + (sum & ((1UL << 32) - 1));
}

int main(void)
{
    uint32_t rtc_freq = *reg32(&__base_regs, CHESHIRE_RTC_FREQ_REG_OFFSET);
    uint64_t reset_freq = clint_get_core_freq(rtc_freq, 2500);
    uart_init(&__base_uart, reset_freq, 115200);

    //printf("---- ICache SPM test ----\r\n");
    //printf("[*] UART initialized\r\n");
    //printf("[*] RTC freq: %u, Core freq: %lu\r\n", rtc_freq, reset_freq);

    // First zero the vspace
    for(int i = 0; i < 512; i++){
        vspace_L2[i].raw = 0UL;
        vspace_L1[i].raw = 0UL;
        vspace_L0[i].raw = 0UL;
    }

    // Identity map 0x0 -> 0xFFFFFFFF (first 4 gigapages)
    // Also give full permissions
    for(int i = 0; i < 4; i++){
        vspace_L2[i].pte.ppn2 = i;
        vspace_L2[i].pte.v = 1;
        vspace_L2[i].pte.r = 1;
        vspace_L2[i].pte.w = 1;
        vspace_L2[i].pte.x = 1;
        vspace_L2[i].pte.a = 1;
        vspace_L2[i].pte.d = 1;
    }

    /// LVL 2 -> LVL 1 --------

    // 0x4000000000
    vspace_L2[256].pte.ppn2 = ((unsigned long int) vspace_L1) >> (12 + 9 + 9);
    vspace_L2[256].pte.ppn1 = ((unsigned long int) vspace_L1) >> (12 + 9);
    vspace_L2[256].pte.ppn0 = ((unsigned long int) vspace_L1) >> 12;
    vspace_L2[256].pte.v = 1;

    /// -----------------------


    /// LVL 1 -> LVL 0 --------
    
    // 0x4010000000
    vspace_L1[128].pte.ppn2 = ((unsigned long int) vspace_L0) >> (12 + 9 + 9);
    vspace_L1[128].pte.ppn1 = ((unsigned long int) vspace_L0) >> (12 + 9);
    vspace_L1[128].pte.ppn0 = ((unsigned long int) vspace_L0) >> 12;
    vspace_L1[128].pte.v = 1;

    /// -----------------------


    // Map the 128 kiB SPM to [0x4010000000, 0x4010010000)
    for(int i = 0; i < 32; i++){

        vspace_L0[i].pte.ppn2 = 0;   // 0x4010000000 -> 0x10000000
        vspace_L0[i].pte.ppn1 = 128;
        vspace_L0[i].pte.ppn0 = i;
        vspace_L0[i].pte.r = 1;
        vspace_L0[i].pte.w = 1;
        vspace_L0[i].pte.x = 1;
        vspace_L0[i].pte.a = 1;
        vspace_L0[i].pte.d = 1;
        vspace_L0[i].pte.v = 1;
    }

    //printf("[*] Page tables initialized\r\n");

    //void *virt_entry = (void *) (VM_OFFSET + (uint64_t) payload);
    void *virt_entry = (void *) payload;

    __asm volatile(
        "csrrw x0, satp, %0\n       \
         csrrw x0, mepc, %1\n       \
         li t0, 1\n                 \
         slli t0, t0, 11\n          \
         csrrs x0, mstatus, t0\n    \
         slli t0, t0, 1\n           \
         csrrc x0, mstatus, t0\n    \
         sfence.vma x0, x0\n        \
         fence.i\n                  \
         csrrwi x0, 0x5E1, 3\n      \
         csrrwi x0, 0x5E0, 15\n     \
         mret\n"
        :: "r"(0x8UL << 60 | ((uint64_t) vspace_L2 >> 12)), "r"(virt_entry)
        : "t0"
    );

    while(1){
    }
}
