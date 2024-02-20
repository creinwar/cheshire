#include "freertos_payload.h"
#include "rv64.h"

#include <stdint.h>

volatile static sv39_pte_u __attribute__((aligned(4096))) vspace_L2[512];
volatile static sv39_pte_u __attribute__((aligned(4096))) vspace_L1_rtos[512];

int main(void)
{
    // First zero the vspace
    for(int i = 0; i < 512; i++){
        vspace_L2[i].raw = 0UL;
        vspace_L1_rtos[i].raw = 0UL;
    }

    // We want the following memory map:
    // 0x10000000 -> 0x10000000     (1G)    RWX     SPM     (Firmware)
    // 0x80000000 -> 0x80000000     (1G)    RWX     DRAM    (FreeRTOS load)
    // 0x60000000 -> 0x81000000     (2M)    RWX     DRAM    (FreeRTOS link)
    // 0x61000000 -> 0x01800000     (2M)    RW      DSPM    (FreeRTOS stack)
    // 0x62000000 -> 0x01A00000     (2M)    RWX     ISPM    (FreeRTOS fast code)

    /// LVL 2 -> LVL 1 --------

    // 0x10000000 -> 0x10000000 (1G)
    vspace_L2[0].pte.ppn2 = 0;
    vspace_L2[0].pte.ppn1 = 0;
    vspace_L2[0].pte.ppn0 = 0;
    vspace_L2[0].pte.r = 1;
    vspace_L2[0].pte.w = 1;
    vspace_L2[0].pte.x = 1;
    vspace_L2[0].pte.a = 1;
    vspace_L2[0].pte.d = 1;
    vspace_L2[0].pte.v = 1;
    vspace_L2[0].pte.v = 1;

    // 0x60000000
    vspace_L2[1].pte.ppn2 = ((unsigned long int) vspace_L1_rtos) >> (12 + 9 + 9);
    vspace_L2[1].pte.ppn1 = ((unsigned long int) vspace_L1_rtos) >> (12 + 9);
    vspace_L2[1].pte.ppn0 = ((unsigned long int) vspace_L1_rtos) >> 12;
    vspace_L2[1].pte.v = 1;

    // 0x80000000 -> 0x80000000 (1G)
    vspace_L2[2].pte.ppn2 = 2;
    vspace_L2[2].pte.ppn1 = 0;
    vspace_L2[2].pte.ppn0 = 0;
    vspace_L2[2].pte.r = 1;
    vspace_L2[2].pte.w = 1;
    vspace_L2[2].pte.x = 1;
    vspace_L2[2].pte.a = 1;
    vspace_L2[2].pte.d = 1;
    vspace_L2[2].pte.v = 1;

    /// -----------------------


    /// LVL 1 -> LVL 0 --------
    
    // 0x60000000 -> 0x81000000 (2M)
    vspace_L1_rtos[256].pte.ppn2 = 2;
    vspace_L1_rtos[256].pte.ppn1 = 8;
    vspace_L1_rtos[256].pte.ppn0 = 0;
    vspace_L1_rtos[256].pte.r = 1;
    vspace_L1_rtos[256].pte.w = 1;
    vspace_L1_rtos[256].pte.x = 1;
    vspace_L1_rtos[256].pte.a = 1;
    vspace_L1_rtos[256].pte.d = 1;
    vspace_L1_rtos[256].pte.v = 1;

    // 0x61000000 -> 0x01800000 (2M)
    vspace_L1_rtos[264].pte.ppn2 = 0;
    vspace_L1_rtos[264].pte.ppn1 = 12;
    vspace_L1_rtos[264].pte.ppn0 = 0;
    vspace_L1_rtos[264].pte.r = 1;
    vspace_L1_rtos[264].pte.w = 1;
    vspace_L1_rtos[264].pte.x = 1;
    vspace_L1_rtos[264].pte.a = 1;
    vspace_L1_rtos[264].pte.d = 1;
    vspace_L1_rtos[264].pte.v = 1;

    // 0x62000000 -> 0x01A00000 (2M)
    vspace_L1_rtos[272].pte.ppn2 = 0;
    vspace_L1_rtos[272].pte.ppn1 = 13;
    vspace_L1_rtos[272].pte.ppn0 = 0;
    vspace_L1_rtos[272].pte.r = 1;
    vspace_L1_rtos[272].pte.w = 1;
    vspace_L1_rtos[272].pte.x = 1;
    vspace_L1_rtos[272].pte.a = 1;
    vspace_L1_rtos[272].pte.d = 1;
    vspace_L1_rtos[272].pte.v = 1;

    /// -----------------------

    (void) freertos_payload;

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
        :: "r"(0x8UL << 60 | ((uint64_t) vspace_L2 >> 12)), "r"(freertos_payload)
        : "t0"
    );

    while(1){
    }
}
