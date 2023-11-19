// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Nicole Narr <narrn@student.ethz.ch>
// Christopher Reinwardt <creinwar@student.ethz.ch>
//
// Just clean dram and halt

#include <stdint.h>

int main(void) {

    /*__asm volatile(
	 "csrrci x0, 0x701, 1\n"
	:::
    );*/

    uint64_t *data = (uint64_t *) 0x80000000;
    for(uint64_t i = 0; i < (1024*1024*1024/(4*8)); i++){
	data[i*4]     = 0;
	data[i*4 + 1] = 0;
	data[i*4 + 2] = 0;
	data[i*4 + 3] = 0;
    }

    while(1){
	__asm volatile("wfi\n" :::);
    }
    return 0;
}
