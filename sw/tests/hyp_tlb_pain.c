#include "micro_vmm.h"

#include "regs/cheshire.h"
#include "dif/uart.h"
#include "dif/clint.h"
#include "params.h"
#include "util.h"
#include "printf.h"

#include <stddef.h>

char vm1_stack[4096];
char vm2_stack[4096];
char vm3_stack[1024];

extern void *__base_regs;

//volatile unsigned int * const scr1 = (unsigned int *) ((unsigned long int) &__base_regs + 0x4);
volatile unsigned int * const scr2 = (unsigned int *) ((unsigned long int) &__base_regs + 0x8);
volatile unsigned int * const scr3 = (unsigned int *) ((unsigned long int) &__base_regs + 0xC);
volatile unsigned int * const scr4 = (unsigned int *) ((unsigned long int) &__base_regs + 0x10);

// Guest stage page tables
static sv39_pte_u __attribute__((aligned(16384), section(".bulk"))) vspace_g_vm1_L2[2048];
static sv39_pte_u __attribute__((aligned(16384), section(".bulk"))) vspace_g_vm2_L2[2048];

// VS mode page tables
static sv39_pte_u __attribute__((aligned(4096), section(".bulk"))) vspace_v_vm1_L2[512];
static sv39_pte_u __attribute__((aligned(4096), section(".bulk"))) vspace_v_vm1_L1[512];
static sv39_pte_u __attribute__((aligned(4096), section(".bulk"))) vspace_v_vm1_L0[512];
static sv39_pte_u __attribute__((aligned(4096), section(".bulk"))) vspace_v_vm2_L2[512];
static sv39_pte_u __attribute__((aligned(4096), section(".bulk"))) vspace_v_vm2_L1[512];

void page_toucher(unsigned long int *base, unsigned long int num_pages)
{
    //char *msg_tx = (char *) 0x100000000;
    //char *msg_rx = (char *) 0x100000008;

    // Touch num_pages 4k pages in order, starting at base
    for(int iter = 0; iter < 1000; iter++){
        for(unsigned long int i = 0; i < num_pages; i++){
            base[i*512] = (unsigned long int) base;
        }
        //*msg_tx = (char) iter;
        
        //__asm volatile("fence" ::: "memory");

        //base[(num_pages-1)*512] = (unsigned long int) *msg_rx;
    }
}

void wait_two_vms(uint32_t *dtlb_misses_vm1, uint32_t *dtlb_misses_vm2)
{
    char vm1_done = 0, vm2_done = 0;
    while(!vm1_done || !vm2_done){

        if(dtlb_misses_vm1 && *dtlb_misses_vm1 && !vm1_done){
            vm1_done = 1;
            printf("[-->] First VM DTLB misses: %lu\r\n", *dtlb_misses_vm1 >> 1);
        }

        if(dtlb_misses_vm2 && *dtlb_misses_vm2 && !vm2_done){
            vm2_done = 1;
            printf("[-->] Second VM DTLB misses: %lu\r\n", *dtlb_misses_vm2 >> 1);
        }
    }

    printf("[*] Done\r\n");

    *scr2 = 1;
    while(1){
    }
}

int main(void)
{
    vm_t vm1, vm2, vm3;

    uint32_t rtc_freq = *reg32(&__base_regs, CHESHIRE_RTC_FREQ_REG_OFFSET);
    uint64_t reset_freq = clint_get_core_freq(rtc_freq, 2500);
    uart_init(&__base_uart, reset_freq, 115200);

    //printf("---- Virtual Machine Switcher ----\r\n");
    //printf("[*] UART initialized\r\n");
    //printf("[*] RTC freq: %u, Core freq: %lu\r\n", rtc_freq, reset_freq);

    // First zero both g stage vspaces
    for(int i = 0; i < 2048; i++){
        vspace_g_vm1_L2[i].raw = 0UL;
        vspace_g_vm2_L2[i].raw = 0UL;
    }

    // Then zero the VS stage vspaces
    for(int i = 0; i < 512; i++){
        vspace_v_vm1_L2[i].raw = 0UL;
        vspace_v_vm1_L1[i].raw = 0UL;
        vspace_v_vm1_L0[i].raw = 0UL;
        vspace_v_vm2_L2[i].raw = 0UL;
        vspace_v_vm2_L1[i].raw = 0UL;
    }

    // Identity map 0x0 -> 0xFFFFFFFF (first 4 gigapages)
    // Also give full permissions
    // For the G-Stage translation we also have to set the U-bit
    // as the g-stage accesses are made as if they came from U-mode
    for(int i = 0; i < 4; i++){
        vspace_g_vm1_L2[i].pte.ppn2 = i;
        vspace_g_vm1_L2[i].pte.v = 1;
        vspace_g_vm1_L2[i].pte.r = 1;
        vspace_g_vm1_L2[i].pte.w = 1;
        vspace_g_vm1_L2[i].pte.x = 1;
        vspace_g_vm1_L2[i].pte.a = 1;
        vspace_g_vm1_L2[i].pte.d = 1;
        vspace_g_vm1_L2[i].pte.u = 1;

        vspace_g_vm2_L2[i].pte.ppn2 = i;
        vspace_g_vm2_L2[i].pte.v = 1;
        vspace_g_vm2_L2[i].pte.r = 1;
        vspace_g_vm2_L2[i].pte.w = 1;
        vspace_g_vm2_L2[i].pte.x = 1;
        vspace_g_vm2_L2[i].pte.a = 1;
        vspace_g_vm2_L2[i].pte.d = 1;
        vspace_g_vm2_L2[i].pte.u = 1;

        vspace_v_vm1_L2[i].pte.ppn2 = i;
        vspace_v_vm1_L2[i].pte.v = 1;
        vspace_v_vm1_L2[i].pte.r = 1;
        vspace_v_vm1_L2[i].pte.w = 1;
        vspace_v_vm1_L2[i].pte.x = 1;
        vspace_v_vm1_L2[i].pte.a = 1;
        vspace_v_vm1_L2[i].pte.d = 1;
        vspace_v_vm1_L2[i].pte.u = 1;

        vspace_v_vm2_L2[i].pte.ppn2 = i;
        vspace_v_vm2_L2[i].pte.v = 1;
        vspace_v_vm2_L2[i].pte.r = 1;
        vspace_v_vm2_L2[i].pte.w = 1;
        vspace_v_vm2_L2[i].pte.x = 1;
        vspace_v_vm2_L2[i].pte.a = 1;
        vspace_v_vm2_L2[i].pte.d = 1;
        vspace_v_vm2_L2[i].pte.u = 1;
    }

    /// LVL 2 -> LVL 1 --------
    // Message virtual devices mapped at 0x1'0000'0000
    vspace_v_vm1_L2[4].pte.ppn2 = 4;
    vspace_v_vm1_L2[4].pte.ppn1 = 0;
    vspace_v_vm1_L2[4].pte.ppn0 = 0;
    vspace_v_vm1_L2[4].pte.v = 1;
    vspace_v_vm1_L2[4].pte.r = 1;
    vspace_v_vm1_L2[4].pte.w = 1;
    vspace_v_vm1_L2[4].pte.x = 0;
    vspace_v_vm1_L2[4].pte.a = 1;
    vspace_v_vm1_L2[4].pte.d = 1;
    vspace_v_vm1_L2[4].pte.u = 1;

    vspace_v_vm2_L2[4].pte.ppn2 = 4;
    vspace_v_vm2_L2[4].pte.ppn1 = 0;
    vspace_v_vm2_L2[4].pte.ppn0 = 0;
    vspace_v_vm2_L2[4].pte.v = 1;
    vspace_v_vm2_L2[4].pte.r = 1;
    vspace_v_vm2_L2[4].pte.w = 1;
    vspace_v_vm2_L2[4].pte.x = 0;
    vspace_v_vm2_L2[4].pte.a = 1;
    vspace_v_vm2_L2[4].pte.d = 1;
    vspace_v_vm2_L2[4].pte.u = 1;

    vspace_v_vm1_L2[10].pte.ppn2 = ((unsigned long int) vspace_v_vm1_L1) >> (12 + 9 + 9);
    vspace_v_vm1_L2[10].pte.ppn1 = ((unsigned long int) vspace_v_vm1_L1) >> (12 + 9);
    vspace_v_vm1_L2[10].pte.ppn0 = ((unsigned long int) vspace_v_vm1_L1) >> 12;
    vspace_v_vm1_L2[10].pte.v = 1;

    vspace_v_vm2_L2[15].pte.ppn2 = ((unsigned long int) vspace_v_vm2_L1) >> (12 + 9 + 9);
    vspace_v_vm2_L2[15].pte.ppn1 = ((unsigned long int) vspace_v_vm2_L1) >> (12 + 9);
    vspace_v_vm2_L2[15].pte.ppn0 = ((unsigned long int) vspace_v_vm2_L1) >> 12;
    vspace_v_vm2_L2[15].pte.v = 1;
    /// -----------------------


    /// LVL 1 -> LVL 0 --------
    vspace_v_vm1_L1[0].pte.ppn2 = ((unsigned long int) vspace_v_vm1_L0) >> (12 + 9 + 9);
    vspace_v_vm1_L1[0].pte.ppn1 = ((unsigned long int) vspace_v_vm1_L0) >> (12 + 9);
    vspace_v_vm1_L1[0].pte.ppn0 = ((unsigned long int) vspace_v_vm1_L0) >> 12;
    vspace_v_vm1_L1[0].pte.v = 1;

    // Instead, vm2 terminates here (i.e. we have a 2M page)
    vspace_v_vm2_L1[0].pte.ppn2 = 2;
    vspace_v_vm2_L1[0].pte.ppn1 = 8;
    vspace_v_vm2_L1[0].pte.ppn0 = 0;
    vspace_v_vm2_L1[0].pte.r    = 1;
    vspace_v_vm2_L1[0].pte.w    = 1;
    vspace_v_vm2_L1[0].pte.x    = 0;
    vspace_v_vm2_L1[0].pte.a    = 1;
    vspace_v_vm2_L1[0].pte.d    = 1;
    vspace_v_vm2_L1[0].pte.u    = 1;
    vspace_v_vm2_L1[0].pte.v    = 1;
    /// -----------------------


    // Also map the same 2M in vm1
    for(int i = 0; i < 512; i++){
        vspace_v_vm1_L0[i].pte.ppn2 = 2;   // 0x280000000 -> 0x81000000
        vspace_v_vm1_L0[i].pte.ppn1 = 8;
        vspace_v_vm1_L0[i].pte.ppn0 = i;
        vspace_v_vm1_L0[i].pte.r = 1;
        vspace_v_vm1_L0[i].pte.w = 1;
        vspace_v_vm1_L0[i].pte.x = 0;
        vspace_v_vm1_L0[i].pte.a = 1;
        vspace_v_vm1_L0[i].pte.d = 1;
        vspace_v_vm1_L0[i].pte.u = 1;
        vspace_v_vm1_L0[i].pte.v = 1;
    }

    //printf("[*] Page tables initialized\r\n");

    // Task 1 gets the memory identity mapped for the code and a giga-page for data accesses
    vm_create(&vm1, 1UL << 7, (2UL << 13),
                    0xA1, 0xA1, 0xFFFF, (void *) vspace_g_vm1_L2, (void *) vspace_v_vm1_L2,
                    (void *) page_toucher, (void *) (vm1_stack + 4096), 0x280000000, 64);

    // Task 2 gets the same identity map, but 2M in 4k pages for data accesses
    vm_create(&vm2, 1UL << 7, (2UL << 13),
                    0xB2, 0xB2, 0xFFFF, (void *) vspace_g_vm2_L2, (void *) vspace_v_vm2_L2,
                    (void *) page_toucher, (void *) (vm2_stack + 4096), 0x3c0000000, 64);

    // VM 3 polls two addresses and if they've been written prints them as DTLB misses
    vm_create(&vm3, 1UL << 7, 2UL << 13,
                    0xC3, 0xC3, 0xFFFF, 0UL, 0UL,
                    (void *) wait_two_vms, (void *) (vm3_stack + 1024), scr3, scr4);


    // Hack in the current global pointer into the state of vm3
    __asm volatile(
        "sd gp, %0\n"
        : "=m"(((saved_state_t *) vm3.sp)->regs.x.x3)
        ::
    );

    //printf("[*] Threads created\r\n");

    unsigned long int lock_vpn = 0, lock_id = (0xB2UL << 15) | (0xB2 << 1) | 1;
    sv39_pte_u lock_pte = {0};

    lock_pte.pte.ppn2 = 2;   // 0x3c0000000 -> 0x81000000
    lock_pte.pte.ppn1 = 8;
    lock_pte.pte.ppn0 = 0;
    lock_pte.pte.r = 1;
    lock_pte.pte.w = 1;
    lock_pte.pte.x = 0;
    lock_pte.pte.a = 1;
    lock_pte.pte.d = 1;
    lock_pte.pte.v = 1;
    lock_pte.pte.u = 1;

    // 2M page, virtual, data and g- and s-stage enabled
    lock_vpn = 0x3c0000000UL | (3 << 5) | (1 << 4) | (1 << 2) | 0x2UL;

    // Lock giga page TLB entry (although the page table says something else)
    /*__asm volatile(
        "csrrw x0, 0x5D8, %0\n      \
         csrrw x0, 0x5D9, %1\n      \
         csrrw x0, 0x5DA, %2\n"
        :: "r"(lock_pte), "r"(lock_vpn), "r"(lock_id)
        :
    );*/

    //printf("[*] PTE locked\r\n");

    // Both vms
    vm_enqueue(&vm1, &vm2);
    vm_enqueue(&vm2, &vm3);

    // Only vm1
    //thread_enqueue(&vm2, &vm3);

    //printf("[*] Kicking off scheduler...\r\n");

    vm_kickoff(&vm1);

    while(1){
    }
}
