// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Nicole Narr <narrn@student.ethz.ch>
// Christopher Reinwardt <creinwar@student.ethz.ch>
//
// Exercise the colouring and locking features

#include <stdint.h>
#include <rv64.h>

#define TLB_NUM_COLOURS 16
#define TLB_NUM_LOCK_ENTRIES 8

// Select to which mode we return

// Supervisor/User mode
//#define TARGET_USER_MODE 1

// Virtualized or not
//#define TARGET_VIRTUAL 1

static sv39_pte_u __attribute__((aligned(4096))) vspace_L2[512];
static sv39_pte_u __attribute__((aligned(4096))) vspace_L1[512];
static sv39_pte_u __attribute__((aligned(4096))) vspace_L0[512];

void page_toucher(unsigned long int *base, unsigned long int num_pages)
{
    for(unsigned long int i = 0; i < num_pages; i++){
        base[i*512] = (unsigned long int) base;
    }
}

void run_colour_test(void)
{
    for(int i = 0; i < TLB_NUM_COLOURS; i++){
        __asm volatile(
            "csrrw x0, 0x5C0, %0\n  \
             sfence.vma x0, x0\n"
            :: "r"(1 << i)
            :
        );
        page_toucher((unsigned long int *) 0x280000000UL, 64);
    }
    __asm volatile(
        "csrrw x0, 0x5C0, %0\n  \
         sfence.vma x0, x0\n"
        :: "r"(0xFFFF)
        :
    );
}

void run_locking_test(void)
{
    unsigned long int lock_vpn = 0, lock_id = 1;
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
#ifdef TARGET_USER_MODE
    lock_pte.pte.u = 1;
#endif

    // 2M page, non-virtual, data and s-stage enabled
    lock_vpn = 0x280000000UL | 0x32UL;

    for(int i = 0; i < TLB_NUM_LOCK_ENTRIES; i++){
        // Lock giga page TLB entry (although the page table says something else)
        switch(i){
            case 0: __asm volatile(
                        "csrrw x0, 0x5C1, %0\n      \
                         csrrw x0, 0x5C2, %1\n      \
                         csrrw x0, 0x5C3, %2\n"
                        :: "r"(lock_pte), "r"(lock_vpn), "r"(lock_id)
                        :
                    );
                    break;
            case 1: __asm volatile(
                        "csrrw x0, 0x5C4, %0\n      \
                         csrrw x0, 0x5C5, %1\n      \
                         csrrw x0, 0x5C6, %2\n"
                        :: "r"(lock_pte), "r"(lock_vpn), "r"(lock_id)
                        :
                    );
                    break;
            case 2: __asm volatile(
                        "csrrw x0, 0x5C7, %0\n      \
                         csrrw x0, 0x5C8, %1\n      \
                         csrrw x0, 0x5C9, %2\n"
                        :: "r"(lock_pte), "r"(lock_vpn), "r"(lock_id)
                        :
                    );
                    break;
            case 3: __asm volatile(
                        "csrrw x0, 0x5CA, %0\n      \
                         csrrw x0, 0x5CB, %1\n      \
                         csrrw x0, 0x5CC, %2\n"
                        :: "r"(lock_pte), "r"(lock_vpn), "r"(lock_id)
                        :
                    );
                    break;
            case 4: __asm volatile(
                        "csrrw x0, 0x5CD, %0\n      \
                         csrrw x0, 0x5CE, %1\n      \
                         csrrw x0, 0x5CF, %2\n"
                        :: "r"(lock_pte), "r"(lock_vpn), "r"(lock_id)
                        :
                    );
                    break;
            case 5: __asm volatile(
                        "csrrw x0, 0x5D0, %0\n      \
                         csrrw x0, 0x5D1, %1\n      \
                         csrrw x0, 0x5D2, %2\n"
                        :: "r"(lock_pte), "r"(lock_vpn), "r"(lock_id)
                        :
                    );
                    break;
            case 6: __asm volatile(
                        "csrrw x0, 0x5D3, %0\n      \
                         csrrw x0, 0x5D4, %1\n      \
                         csrrw x0, 0x5D5, %2\n"
                        :: "r"(lock_pte), "r"(lock_vpn), "r"(lock_id)
                        :
                    );
                    break;
            case 7: __asm volatile(
                        "csrrw x0, 0x5D6, %0\n      \
                         csrrw x0, 0x5D7, %1\n      \
                         csrrw x0, 0x5D8, %2\n"
                        :: "r"(lock_pte), "r"(lock_vpn), "r"(lock_id)
                        :
                    );
                    break;
            default: break;
        }

        page_toucher((unsigned long int *) 0x280000000UL, 64);

        switch(i){
            case 0: __asm volatile(
                        "csrrw x0, 0x5C1, %0\n      \
                         csrrw x0, 0x5C2, %1\n      \
                         csrrw x0, 0x5C3, %2\n"
                        :: "r"(0), "r"(0), "r"(0)
                        :
                    );
                    break;
            case 1: __asm volatile(
                        "csrrw x0, 0x5C4, %0\n      \
                         csrrw x0, 0x5C5, %1\n      \
                         csrrw x0, 0x5C6, %2\n"
                        :: "r"(0), "r"(0), "r"(0)
                        :
                    );
                    break;
            case 2: __asm volatile(
                        "csrrw x0, 0x5C7, %0\n      \
                         csrrw x0, 0x5C8, %1\n      \
                         csrrw x0, 0x5C9, %2\n"
                        :: "r"(0), "r"(0), "r"(0)
                        :
                    );
                    break;
            case 3: __asm volatile(
                        "csrrw x0, 0x5CA, %0\n      \
                         csrrw x0, 0x5CB, %1\n      \
                         csrrw x0, 0x5CC, %2\n"
                        :: "r"(0), "r"(0), "r"(0)
                        :
                    );
                    break;
            case 4: __asm volatile(
                        "csrrw x0, 0x5CD, %0\n      \
                         csrrw x0, 0x5CE, %1\n      \
                         csrrw x0, 0x5CF, %2\n"
                        :: "r"(0), "r"(0), "r"(0)
                        :
                    );
                    break;
            case 5: __asm volatile(
                        "csrrw x0, 0x5D0, %0\n      \
                         csrrw x0, 0x5D1, %1\n      \
                         csrrw x0, 0x5D2, %2\n"
                        :: "r"(0), "r"(0), "r"(0)
                        :
                    );
                    break;
            case 6: __asm volatile(
                        "csrrw x0, 0x5D3, %0\n      \
                         csrrw x0, 0x5D4, %1\n      \
                         csrrw x0, 0x5D5, %2\n"
                        :: "r"(0), "r"(0), "r"(0)
                        :
                    );
                    break;
            case 7: __asm volatile(
                        "csrrw x0, 0x5D6, %0\n      \
                         csrrw x0, 0x5D7, %1\n      \
                         csrrw x0, 0x5D8, %2\n"
                        :: "r"(0), "r"(0), "r"(0)
                        :
                    );
                    break;
            default: break;
        }
    }
}

int main(void) {

    // Clean pagetables
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
#ifdef TARGET_USER_MODE
        vspace_L2[i].pte.u = 1;
#endif
    }

    // Create additional 4k mappings
    /// LVL 2 -> LVL 1 --------
    vspace_L2[10].pte.ppn2 = ((unsigned long int) vspace_L1) >> (12 + 9 + 9);
    vspace_L2[10].pte.ppn1 = ((unsigned long int) vspace_L1) >> (12 + 9);
    vspace_L2[10].pte.ppn0 = ((unsigned long int) vspace_L1) >> 12;
    vspace_L2[10].pte.v = 1;

    /// LVL 1 -> LVL 0 --------
    vspace_L1[0].pte.ppn2 = ((unsigned long int) vspace_L0) >> (12 + 9 + 9);
    vspace_L1[0].pte.ppn1 = ((unsigned long int) vspace_L0) >> (12 + 9);
    vspace_L1[0].pte.ppn0 = ((unsigned long int) vspace_L0) >> 12;
    vspace_L1[0].pte.v = 1;

    // Map the entire L0 page table
    for(int i = 0; i < 512; i++){
        vspace_L0[i].pte.ppn2 = 2;   // 0x280000000 -> 0x80000000
        vspace_L0[i].pte.ppn0 = i;
        vspace_L0[i].pte.r = 1;
        vspace_L0[i].pte.w = 1;
        vspace_L0[i].pte.x = 0;
        vspace_L0[i].pte.a = 1;
        vspace_L0[i].pte.d = 1;
        vspace_L0[i].pte.v = 1;
#ifdef TARGET_USER_MODE
        vspace_L0[i].pte.u = 1;
#endif
    }

    uint64_t satp = 8UL << 60 | ((unsigned long int) vspace_L2) >> 12;

    // For (H)S- and U-Mode use SATP
    // For   VS- and VU-mode use VSATP
    __asm volatile(
        "csrrw x0, satp, %0\n   \
         csrrw x0, vsatp, %0\n"
        :: "r"(satp)
        :
    );

    uint64_t mstatus_set = 0, mstatus_clear = 0;


#ifndef TARGET_USER_MODE
#ifndef TARGET_VIRTUAL
    // Drop back to (H)S-Mode and run the tests
    // MPP = 01, MPIE = 0, MPV = 0
    mstatus_set     = 1UL << 11;
    mstatus_clear   = 1UL << 39 | 1UL << 12 | 1UL << 7;
#endif
#endif

#ifdef TARGET_USER_MODE
#ifndef TARGET_VIRTUAL
    // Drop back to U-Mode and check for failure
    // MPP = 00, MPIE = 0, MPV = 0
    mstatus_set     = 0UL;
    mstatus_clear   = 1UL << 39 | 3UL << 11 | 1UL << 7;
#endif
#endif
    
#ifndef TARGET_USER_MODE
#ifdef TARGET_VIRTUAL
    // Drop back to VS-Mode and check for failure
    // MPP = 01, MPIE = 0, MPV = 1
    mstatus_set     = 1UL << 39 | 1UL << 11;
    mstatus_clear   = 1UL << 12 | 1UL << 7;
#endif
#endif

#ifdef TARGET_USER_MODE
#ifdef TARGET_VIRTUAL
    // Drop back to VU-Mode and check for failure
    // MPP = 00, MPIE = 0, MPV = 1
    mstatus_set     = 1UL << 39;
    mstatus_clear   = 3UL << 11 | 1UL << 7;
#endif
#endif
    __asm volatile(
        "csrrs x0, mstatus, %0\n    \
         csrrc x0, mstatus, %1\n    \
         auipc t0, 0\n              \
         addi t0, t0, 14\n           \
         csrrw x0, mepc, t0\n       \
         mret\n"
         :: "r"(mstatus_set), "r"(mstatus_clear)
         : "t0"
    );

    run_colour_test();

    run_locking_test();

    return 0;
}
