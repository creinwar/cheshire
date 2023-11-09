#ifndef RV64_H_
#define RV64_H_

typedef union {
    unsigned long int raw[32];
    struct {
        unsigned long int pc;
        unsigned long int x1;
        unsigned long int x2;
        unsigned long int x3;
        unsigned long int x4;
        unsigned long int x5;
        unsigned long int x6;
        unsigned long int x7;
        unsigned long int x8;
        unsigned long int x9;
        unsigned long int x10;
        unsigned long int x11;
        unsigned long int x12;
        unsigned long int x13;
        unsigned long int x14;
        unsigned long int x15;
        unsigned long int x16;
        unsigned long int x17;
        unsigned long int x18;
        unsigned long int x19;
        unsigned long int x20;
        unsigned long int x21;
        unsigned long int x22;
        unsigned long int x23;
        unsigned long int x24;
        unsigned long int x25;
        unsigned long int x26;
        unsigned long int x27;
        unsigned long int x28;
        unsigned long int x29;
        unsigned long int x30;
        unsigned long int x31;
    } x;
} rv64_int_regs_u;

typedef union {
    unsigned long int raw[33];
    struct {
        unsigned long int f0;
        unsigned long int f1;
        unsigned long int f2;
        unsigned long int f3;
        unsigned long int f4;
        unsigned long int f5;
        unsigned long int f6;
        unsigned long int f7;
        unsigned long int f8;
        unsigned long int f9;
        unsigned long int f10;
        unsigned long int f11;
        unsigned long int f12;
        unsigned long int f13;
        unsigned long int f14;
        unsigned long int f15;
        unsigned long int f16;
        unsigned long int f17;
        unsigned long int f18;
        unsigned long int f19;
        unsigned long int f20;
        unsigned long int f21;
        unsigned long int f22;
        unsigned long int f23;
        unsigned long int f24;
        unsigned long int f25;
        unsigned long int f26;
        unsigned long int f27;
        unsigned long int f28;
        unsigned long int f29;
        unsigned long int f30;
        unsigned long int f31;
        unsigned long int fcsr;
    } fp;
} rv64_fp_regs_u;

typedef union {
    unsigned long int raw[7];
    struct {
        unsigned long int cur_clrs;
        unsigned long int sstatus;
        unsigned long int satp;
        unsigned long int hstatus;
        unsigned long int hgatp;
        unsigned long int vsstatus;
        unsigned long int vsatp;
    } x;
} rv64_csrs_u;

typedef struct {
    rv64_int_regs_u regs;
    rv64_fp_regs_u fp_regs;
    rv64_csrs_u csrs;
} saved_state_t;

typedef union {
    unsigned long int raw;
    struct {
        unsigned long int v:1;
        unsigned long int r:1;
        unsigned long int w:1;
        unsigned long int x:1;
        unsigned long int u:1;
        unsigned long int g:1;
        unsigned long int a:1;
        unsigned long int d:1;
        unsigned long int rsw:2;
        unsigned long int ppn0:9;
        unsigned long int ppn1:9;
        unsigned long int ppn2:26;
        unsigned long int resv:7;
        unsigned long int pbmt:2;
        unsigned long int n:1;
    } pte;
} sv39_pte_u;

#endif