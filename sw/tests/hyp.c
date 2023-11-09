
void hs_handler(void)
{
    while(1){
        __asm volatile("wfi\n" :::);
    }
}


// Basic hypervisor tryout
// 1. Drop from M-mode to VS mode
// 2. Set "sstatus" (which means vsstatus)
// 3. Trigger an ecall (should go to M-mode)
//
// This should allow a basic understanding using simulation
// and inspection of mstatus, hsstatus, vsstatus, ...
int main(void)
{

    // Drop to VS mode and delegate VS-mode ecalls to HS mode
    // i.e. MPV = 1, MPP = 1
    // stval = hs handler, medeleg = 1 << 10 and the mret
    __asm volatile(
        "csrrs a0, misa, x0\n       \
         li t0, 1\n                 \
         slli t0, t0, 11\n          \
         csrrs x0, mstatus, t0\n    \
         slli t0, t0, 1\n           \
         csrrc x0, mstatus, t0\n    \
         slli t0, t0, 27\n          \
         csrrs x0, mstatus, t0\n    \
         csrrw x0, stvec, %0\n      \
         li t0, 1\n                 \
         slli t0, t0, 10\n          \
         csrrs x0, medeleg, t0\n    \
         auipc t0, 0\n              \
         addi t0, t0, 14\n           \
         csrrw x0, mepc, t0\n       \
         mret\n"
        :: "r"(hs_handler)
        : "t0"
    );

    // Should be in VS mode now, so lets set something in sstatus
    // and perform an ecall to get back to m-mode
    __asm volatile(
        "li t0, 2\n             \
        csrrs x0, sstatus, t0\n \
        addi a0, x0, 69\n       \
        ecall\n                 \
        add x0, x0, x0\n        \
        add x0, x0, x0\n        \
        add x0, x0, x0\n        \
        add x0, x0, x0\n"
        ::: "t0"
    );

    while(1){
    }
}
