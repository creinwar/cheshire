// Forward machine timer interrupts, read mhpmcounters on ecall 13 and die on anything else
rv64_vm_m_handler:

    sd t0, -8(sp)
    sd t1, -16(sp)    

    li t0, 1
    slli t0, t0, 63
    addi t0, t0, 7
    csrrs t1, mcause, x0
    xor t0, t0, t1

    // If this is not a machine mode timer interrupt we have to check for an ecall
    bne t0, x0, m_mode_check_ecall

    // Set mtimecmphigh to the max
    li t0, -1
    li t1, 2
    slli t1, t1, 8
    addi t1, t1, 4
    slli t1, t1, 4
    addi t1, t1, 4
    slli t1, t1, 12
    sw t0, 4(t1)
    sw t0, 0(t1)

    fence

    // Forward the timer interrupt to HS mode
    li t0, 1
    slli t0, t0, 63
    addi t0, t0, 5
    csrrw x0, scause, t0
    csrrs t0, mepc, x0
    csrrw x0, sepc, t0

    // "Return" to HS handler
    csrrs t0, stvec, x0
    csrrw x0, mepc, t0

    // Update sstatus SIE, SPIE and SPP
    csrrs t0, sstatus, x0
    andi t0, t0, 2
    csrrc x0, sstatus, t0
    slli t0, t0, 3
    csrrs x0, sstatus, t0
    csrrs t0, mstatus, t0
    srli t0, t0, 11
    andi t0, t0, 1
    beq t0, x0, m_mode_u_pp
    slli t0, t0, 8
    csrrs x0, sstatus, t0
    jal x0, m_mode_drop_s

m_mode_u_pp:
    li t0, 1
    slli t0, t0, 8
    csrrc x0, sstatus, t0

m_mode_drop_s:
    li t0, 1
    slli t0, t0, 11
    csrrs x0, mstatus, t0   // Drop to S mode

    // Update hstatus with SPV and clear it in mstatus
    // We want to return to HS not VS
    csrrs t0, mstatus, x0
    srli t0, t0, 39
    andi t0, t0, 1
    slli t0, t0, 7
    csrrs x0, hstatus, t0
    slli t0, t0, 32
    csrrc x0, mstatus, t0

    // Read/clear the current mhpmcounter3 and pass it in sscratch
    // to the hs handler
    csrrw t0, mhpmcounter3, x0
    csrrw x0, sscratch, t0

    jal x0, m_mode_exit

m_mode_check_ecall:
    // Assuming t1 contains mcause
    // Range from 8-11 are ecalls, we service all of them
    li t0, 8
    blt t1, t0, m_mode_death
    li t0, 12
    bge t1, t0, m_mode_death

    // ECALL 13 is the mhpmcounter read
    li t0, 13
    bne a0, t0, m_mode_death

    csrrw a0, mhpmcounter3, x0

    // Jump to after the ecall
    csrrs t0, mepc, x0
    addi t0, t0, 4
    csrrw x0, mepc, t0

m_mode_exit:

    // Restore registers
    ld t0, -8(sp)
    ld t1, -16(sp)

    // And return
    mret

m_mode_death:
    wfi
    jal x0, rv64_vm_m_handler

// If this was a timer interrupt:
//      Save context, call schedule,
//      re-enable timer interrupt, switch to new vm
//
// If not:
//      Cry and do nothing :(
.align 8
rv64_vm_hs_handler:
    // Make space for the I-Regs, FP-Regs and CSRs
    addi sp, sp, -576
    sd ra, 8(sp)

    // Save exception pc
    csrrs ra, sepc, x0
    sd ra, 0(sp)

    // Save old sp
    addi ra, sp, 576
    sd ra, 16(sp)

    // Skip ra as we saved that already
    // Skip sp as it was preserved as well
    sd x3,   24(sp)
    sd x4,   32(sp)
    sd x5,   40(sp)
    sd x6,   48(sp)
    sd x7,   56(sp)
    sd x8,   64(sp)
    sd x9,   72(sp)
    sd x10,  80(sp)
    sd x11,  88(sp)
    sd x12,  96(sp)
    sd x13, 104(sp)
    sd x14, 112(sp)
    sd x15, 120(sp)
    sd x16, 128(sp)
    sd x17, 136(sp)
    sd x18, 144(sp)
    sd x19, 152(sp)
    sd x20, 160(sp)
    sd x21, 168(sp)
    sd x22, 176(sp)
    sd x23, 184(sp)
    sd x24, 192(sp)
    sd x25, 200(sp)
    sd x26, 208(sp)
    sd x27, 216(sp)
    sd x28, 224(sp)
    sd x29, 232(sp)
    sd x30, 240(sp)
    sd x31, 248(sp) 

    // a0 -> FP Regs
    // a1 -> CSRs
    addi a0, sp, 256
    addi a1, a0, 264

    // Save current partitioning colour
    csrrs t0, 0x5C0, x0
    sd t0, 0(a1)

    // Save hstatus
    csrrs t0, hstatus, x0
    sd t0, 24(a1)

    // Save hgatp
    csrrs t0, hgatp, x0
    sd t0, 32(a1)

    // Save vsstatus
    csrrs t0, vsstatus, x0
    sd t0, 40(a1)

    // Save vsatp
    csrrs t0, vsatp, x0
    sd t0, 48(a1)

    // Figure out whether this was actually a timer interrupt or ecall
    csrrs t0, scause, x0
    add t1, t0, x0

    // Check highest bit => If it's set, this was an interrupt
    // Otherwise an exception
    srli t0, t0, 63
    beq t0, x0, we_dead

intr:
    // Get rid of the top bit
    slli t1, t1, 1
    
    // Supervisor mode timer interrupt => id 5
    // Shifted once to the left => 10
    addi t0, x0, 10

    // If it was no timer interrupt, we're done
    bne t0, t1, we_dead

cont_save:

    // Now figure out whether we have to save fp state
    // Check for FP state not equal to disabled
    csrrs t2, vsstatus, x0
    srli t3, t2, 13
    andi t3, t3, 3

    beq t3, x0, fp_saved

    // Save the fp state for the thread
    fsd f0,    0(a0)
    fsd f1,    8(a0)
    fsd f2,   16(a0)
    fsd f3,   24(a0)
    fsd f4,   32(a0)
    fsd f5,   40(a0)
    fsd f6,   48(a0)
    fsd f7,   56(a0)
    fsd f8,   64(a0)
    fsd f9,   72(a0)
    fsd f10,  80(a0)
    fsd f11,  88(a0)
    fsd f12,  96(a0)
    fsd f13, 104(a0)
    fsd f14, 112(a0)
    fsd f15, 120(a0)
    fsd f16, 128(a0)
    fsd f17, 136(a0)
    fsd f18, 144(a0)
    fsd f19, 152(a0)
    fsd f20, 160(a0)
    fsd f21, 168(a0)
    fsd f22, 176(a0)
    fsd f23, 184(a0)
    fsd f24, 192(a0)
    fsd f25, 200(a0)
    fsd f26, 208(a0)
    fsd f27, 216(a0)
    fsd f28, 224(a0)
    fsd f29, 232(a0)
    fsd f30, 240(a0)
    fsd f31, 248(a0)

    csrrs t0, fcsr, x0
    sd t0,   256(a0)

fp_saved:

    // Update the stack pointer in the current thread struct
    la t0, cur_vm
    ld t1, 0(t0)
    sd sp, 8(t1)

    // Call trace utility to read out metrics if needed
    //jal ra, vm_trace

    // We're passed the last value of mhpmcounter3 in sscratch
    // This adds it to the dtlb misses in the current thread
    csrrw t2, sscratch, x0
    ld t3, 48(t1)
    add t2, t2, t3
    sd t2, 48(t1)

    // Call schedule to find next task
    jal ra, vm_schedule

    // Switch the stack pointer to the new one
    ld sp, 8(a0)

    // Set next time slice interrupt
    jal ra, vm_set_timer

    // sp -> I regs
    // a0 -> FP regs
    // a1 -> CSRs
    addi a0, sp, 256
    addi a1, a0, 264

    // Restore sepc to return to the next VM
    ld t1, 0(sp)
    csrrw x0, sepc, t1

    // Set current partitioning colour
    ld t0, 0(a1)
    csrrw x0, 0x5C0, t0

    // Restore hstatus
    ld t1, 24(a1)
    csrrw x0, hstatus, t1

    // Restore hgatp
    ld t1, 32(a1)
    csrrw x0, hgatp, t1

    // And the vsstatus of the VM we're changing to
    ld t1, 40(a1)
    csrrw x0, vsstatus, t1

    // Restore vsatp
    ld t2, 48(a1)
    csrrw x0, vsatp, t2

    //sfence.vma x0, x0

    // If the FP extension is not disabled, we have to restore the state
    srli t2, t1, 13
    andi t2, t2, 3

    beq t2, x0, fp_restored

    fld f0,    0(a0)
    fld f1,    8(a0)
    fld f2,   16(a0)
    fld f3,   24(a0)
    fld f4,   32(a0)
    fld f5,   40(a0)
    fld f6,   48(a0)
    fld f7,   56(a0)
    fld f8,   64(a0)
    fld f9,   72(a0)
    fld f10,  80(a0)
    fld f11,  88(a0)
    fld f12,  96(a0)
    fld f13, 104(a0)
    fld f14, 112(a0)
    fld f15, 120(a0)
    fld f16, 128(a0)
    fld f17, 136(a0)
    fld f18, 144(a0)
    fld f19, 152(a0)
    fld f20, 160(a0)
    fld f21, 168(a0)
    fld f22, 176(a0)
    fld f23, 184(a0)
    fld f24, 192(a0)
    fld f25, 200(a0)
    fld f26, 208(a0)
    fld f27, 216(a0)
    fld f28, 224(a0)
    fld f29, 232(a0)
    fld f30, 240(a0)
    fld f31, 248(a0)

    lw t0,   256(a0)
    csrrw x0, fcsr, t0

fp_restored:

    ld x1,    8(sp)
    // Skip sp for now
    ld x3,   24(sp)
    ld x4,   32(sp)
    ld x5,   40(sp)
    ld x6,   48(sp)
    ld x7,   56(sp)
    ld x8,   64(sp)
    ld x9,   72(sp)
    ld x10,  80(sp)
    ld x11,  88(sp)
    ld x12,  96(sp)
    ld x13, 104(sp)
    ld x14, 112(sp)
    ld x15, 120(sp)
    ld x16, 128(sp)
    ld x17, 136(sp)
    ld x18, 144(sp)
    ld x19, 152(sp)
    ld x20, 160(sp)
    ld x21, 168(sp)
    ld x22, 176(sp)
    ld x23, 184(sp)
    ld x24, 192(sp)
    ld x25, 200(sp)
    ld x26, 208(sp)
    ld x27, 216(sp)
    ld x28, 224(sp)
    ld x29, 232(sp)
    ld x30, 240(sp)
    ld x31, 248(sp)

    // Restore a0
    ld sp,   16(sp)

    sret

we_dead:
    wfi
    j we_dead

.global rv64_vm_kickoff
rv64_vm_kickoff:
    // Globally disable interrupts
    addi t0, x0, 8
    csrrc x0, mstatus, t0

    // Enable the machine mode timer interrupt (bit 7)
    slli t0, t0, 4
    csrrs x0, mie, t0

    // Get the struct of the current vm
    la a0, cur_vm
    ld a0, 0(a0)
    ld sp, 8(a0)        // The state was stored on the stack - sp -> I regs
    addi a0, sp, 256    // a0 -> FP regs
    addi a1, a0, 264    // a1 -> CSR regs

    // Insert the M mode handler
    // ... and the HS mode vm scheduler
    la t0, rv64_vm_m_handler
    la t1, rv64_vm_hs_handler
    csrrw x0, mtvec, t0
    csrrw x0, stvec, t1

    // Set current partitioning colour
    ld t0, 0(a1)
    csrrw x0, 0x5C0, t0

    // Restore hstatus
    ld t1, 24(a1)
    csrrw x0, hstatus, t1

    // Restore hgatp
    ld t1, 32(a1)
    csrrw x0, hgatp, t1

    // And the vsstatus of the VM we're changing to
    ld t1, 40(a1)
    csrrw x0, vsstatus, t1

    // Restore vsatp
    ld t1, 48(a1)
    csrrw x0, vsatp, t1

    // Configure mstatus to "return" to VS- or VU-mode with
    // interrupts enabled (i.e. MPIE = 1, MPP = 01/00, MV = 1)
    // Setting bits 7, 11 and 39
    // Clearing bit 12
    li t0, 1
    slli t1, t0, 7
    //slli t2, t0, 11 // VS-mode
    addi t2, x0, 0  // VU-mode
    or t1, t1, t2
    slli t3, t0, 39
    or t3, t3, t1
    csrrs x0, mstatus, t3
    slli t1, t0, 12
    slli t2, t0, 11 // VU-mode
    or t1, t1, t2   // VU-mode
    csrrc x0, mstatus, t1

    // Enable interrupts globally in sstatus
    csrrsi x0, sstatus, 2

    // And enable the timer interrupt in HS mode
    li t0, 32
    csrrs x0, sie, t0

    // Delegate the timer interrupt to HS mode
    csrrs x0, mideleg, t0

    // Set exception return register to PC
    ld t0,  0(sp)
    csrrw x0, mepc, t0

    // Enable DTLB performance counter for mhpmcounter3
    addi t0, x0, 4
    csrrw x0, mhpmevent3, t0

    // If the FP extension is not disabled, we have to restore the state
    ld t1, 40(a1)
    srli t2, t1, 13
    andi t2, t2, 3

    beq t2, x0, vm_kickoff_fp_restored

    fld f0,    0(a0)
    fld f1,    8(a0)
    fld f2,   16(a0)
    fld f3,   24(a0)
    fld f4,   32(a0)
    fld f5,   40(a0)
    fld f6,   48(a0)
    fld f7,   56(a0)
    fld f8,   64(a0)
    fld f9,   72(a0)
    fld f10,  80(a0)
    fld f11,  88(a0)
    fld f12,  96(a0)
    fld f13, 104(a0)
    fld f14, 112(a0)
    fld f15, 120(a0)
    fld f16, 128(a0)
    fld f17, 136(a0)
    fld f18, 144(a0)
    fld f19, 152(a0)
    fld f20, 160(a0)
    fld f21, 168(a0)
    fld f22, 176(a0)
    fld f23, 184(a0)
    fld f24, 192(a0)
    fld f25, 200(a0)
    fld f26, 208(a0)
    fld f27, 216(a0)
    fld f28, 224(a0)
    fld f29, 232(a0)
    fld f30, 240(a0)
    fld f31, 248(a0)

    lw t0,   256(a0)
    csrrw x0, fcsr, t0

vm_kickoff_fp_restored:

    // Make sure everything completed until here
    fence.i

    jal ra, vm_set_timer

    ld x1,    8(sp)
    // Skip SP (x2)
    ld x3,   24(sp)
    ld x4,   32(sp)
    ld x5,   40(sp)
    ld x6,   48(sp)
    ld x7,   56(sp)
    ld x8,   64(sp)
    ld x9,   72(sp)
    ld x10,  80(sp)
    ld x11,  88(sp)
    ld x12,  96(sp)
    ld x13, 104(sp)
    ld x14, 112(sp)
    ld x15, 120(sp)
    ld x16, 128(sp)
    ld x17, 136(sp)
    ld x18, 144(sp)
    ld x19, 152(sp)
    ld x20, 160(sp)
    ld x21, 168(sp)
    ld x22, 176(sp)
    ld x23, 184(sp)
    ld x24, 192(sp)
    ld x25, 200(sp)
    ld x26, 208(sp)
    ld x27, 216(sp)
    ld x28, 224(sp)
    ld x29, 232(sp)
    ld x30, 240(sp)
    ld x31, 248(sp)

    // Restore sp
    ld x2, 16(sp)

    sfence.vma x0, x0

    // "Return" to the entrypoint, restoring the mstatus
    mret
