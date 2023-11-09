// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Nicole Narr <narrn@student.ethz.ch>
// Christopher Reinwardt <creinwar@student.ethz.ch>
//
// Simple payload to test bootmodes

extern void *__base_clint;

volatile unsigned int * const mtimelow = (unsigned int *) ((unsigned long int) &__base_clint + 0xbff8UL);
volatile unsigned int * const mtimehigh = (unsigned int *) ((unsigned long int) &__base_clint + 0xbffcUL);

volatile unsigned int * const mtimecmp0low = (unsigned int *) ((unsigned long int) &__base_clint + 0x4000UL);
volatile unsigned int * const mtimecmp0high = (unsigned int *) ((unsigned long int) &__base_clint + 0x4004UL);

volatile unsigned long clic_base = 0x08000000UL;

void set_next_timer(void)
{
    unsigned long int cur_mtime = 0;

    do {
        cur_mtime = ((unsigned long int) *mtimehigh) << 32;
    } while ((cur_mtime >> 32) != ((unsigned long int) *mtimehigh));

    cur_mtime |= (unsigned long int) *mtimelow + 5;

    *mtimecmp0low  = (unsigned int) cur_mtime;
    *mtimecmp0high = (unsigned int) (cur_mtime >> 32);
}


void __attribute__((naked, aligned(64))) irq_handler(void)
{
    __asm__ volatile(
        "jalr ra, %0\n          \
         addi sp, sp, -16\n     \
         sd sp, 0(sp)\n         \
         addi a0, x0, 3\n       \
         addi a1, a0, 7 /* EX-EX forwarding */\n   \
         sd a0, 8(sp)\n         \
         add x0, x0, x0\n       \
         add x0, x0, x0\n       \
         add x0, x0, x0\n       \
         add x0, x0, x0\n       \
         add x0, x0, x0\n       \
         add x0, x0, x0\n       \
         ld a0, 8(sp)\n         \
         addi a3, a0, 7 /* MEM-EX forwarding */\n   \
sleep_loop:\n                   \
         jal x0, sleep_loop\n"
         :: "r"(set_next_timer)
         :
    );
}

static inline void writeb(char b, void *dest)
{
    __asm volatile(
        "sb %0, 0(%1)\n"
        :: "r"(b), "r"(dest)
        :
    );
}

int main(void) {

    // Non-vectored CLINT mode
    __asm volatile(
        "csrrw x0, mtvec, %0\n"
         :: "r"(irq_handler)
         :
    );

    // Vectored CLINT mode
    //__asm volatile(
    //    "csrrw x0, mtvec, %0\n"
    //     :: "r"(irq_handler + 1)
    //     :
    //);

    // CLIC mode
    //__asm volatile(
    //    "csrrw x0, mtvec, %0\n"
    //     :: "r"(irq_handler + 3)
    //     :
    //);

    set_next_timer();

    unsigned int mtimer_settings = 0xAAC20100;

    //__asm volatile (
    //    "sw %0, 0(%1)\n"
    //    :: "r"(mtimer_settings), "r"(clic_base + 0x1000 + 4*7)
    //    :
    //);

    while(1){
        __asm volatile(
            "csrrsi x0, mstatus, 8\n    \
             addi t0, x0, 1\n           \
             slli t0, t0, 7\n           \
             csrrs x0, mie, t0\n        \
             spin_lbl: jal x0, spin_lbl\n"
            ::: "t0"
        );
    }

    return 0;
}
