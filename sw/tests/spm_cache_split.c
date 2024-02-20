// Copyright 2024 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Christopher Reinwardt <creinwar@student.ethz.ch>
//
// Simple machine mode test for the split SPM/DCache

#include <stdint.h>

#include "regs/cheshire.h"
#include "dif/uart.h"
#include "dif/clint.h"
#include "params.h"
#include "util.h"
#include "printf.h"

volatile uint64_t cache_array[1024];
volatile uint64_t *sys_brom = (uint64_t *) 0x02000000;

extern void __stack_pointer$;

void __attribute__((noinline)) move_stack(void *old_stack_top, void *new_stack_top)
{
    uint64_t *old_64    = (uint64_t *) (old_stack_top - sizeof(uint64_t));
    uint64_t *new_64    = (uint64_t *) (new_stack_top - sizeof(uint64_t));
    uint8_t *old_8      = (uint8_t *)  (old_stack_top - sizeof(uint64_t));
    uint8_t *new_8      = (uint8_t *) (new_stack_top - sizeof(uint64_t));

    uint64_t *cur_sp = 0;

    __asm volatile(
        "add %0, sp, x0\n"
        : "=r"(cur_sp)
        ::
    );

    // Copy over the old stack to the new place
    while(old_8 != cur_sp){
        if((uint64_t) old_8 - (uint64_t) cur_sp >= sizeof(uint64_t)){
            *new_64-- = *old_64--;
            old_8  -= sizeof(uint64_t);
            new_8  -= sizeof(uint64_t);
        } else {
            *new_8-- = *old_8--;
        }
    }

    // Now we change to the new stack
    __asm volatile(
        "add sp, x0, %0\n"
        :: "r"(new_8)
        :
    );

}

int main(void) {
    void *spm_stack = (void *) (-1024LL*1024LL + 16*1024ULL);

    // Enable a 50:50 SPM cache split
    __asm volatile(
        "fence\n                \
         li t0, 15\n            \
         csrrw x0, 0x5E0, t0\n"
        ::: "t0"
    );

    move_stack((void *) &__stack_pointer$, spm_stack);

    // Initialize cache array
    for(int i = 0; i < 1024; i++){
        cache_array[i] = sys_brom[i];
    }

    // Now access different cachelines
    uint64_t sum = 0;
    for(int i = 0; i < 256; i++){
        sum += cache_array[2*i];
    }

    // The SPM starts 1 MiB below the top address
    volatile uint64_t *dspm = (uint64_t *) (-1024LL*1024LL);
    volatile uint32_t *wspm = (uint32_t *) (-1024LL*1024LL);
    volatile uint16_t *hspm = (uint16_t *) (-1024LL*1024LL);
    volatile uint8_t  *bspm = (uint8_t  *) (-1024LL*1024LL);

    *dspm = 0xDEADBEEFBEEFDEADUL;
    *wspm = 0xABBABAABUL;
    *hspm = 0xABBA;
    *bspm = 0xDE;

    for(int i = 0; i < 1024; i++){
        dspm[i] = cache_array[i];
    }

    for(int i = 0; i < 1024; i++){
        if(dspm[i] != sys_brom[i]) {
            __asm volatile("wfi\n" :::);
        }
    }

    uint32_t rtc_freq = *reg32(&__base_regs, CHESHIRE_RTC_FREQ_REG_OFFSET);
    uint64_t reset_freq = clint_get_core_freq(rtc_freq, 2500);
    uart_init(&__base_uart, reset_freq, 115200);

    printf("*dspm = 0x%lx\r\n", *dspm);

    // Now try to cause colisions (i.e. index stays the same but tag changes)
    /*volatile uint64_t cbuf[16*512];
    for(int i = 0; i < 16; i++){
        cbuf[512*i] = sys_brom[i*8];
        cbuf[512*i+1] = sys_brom[i*8 + 1];
        cbuf[512*i+2] = sys_brom[i*8 + 2];
        cbuf[512*i+3] = sys_brom[i*8 + 3];
    }


    uint64_t sum0 = 0, sum1 = 0, sum2 = 0, sum3 = 0;
    for(int i = 0; i < 16; i++){
        sum0 += cbuf[512*i];
        sum1 += cbuf[512*i+1];
        sum2 += cbuf[512*i+2];
        sum3 += cbuf[512*i+3];
    }

    sum += sum0 + sum1 + sum2 + sum3;*/

    while(1){
	    __asm volatile("wfi\n" :::);
    }

    return 0;
}
