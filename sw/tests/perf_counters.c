#include "printf.h"


unsigned long int buf[512];

void copy_8bytes(unsigned long int *src, unsigned long int *dst, unsigned long int num)
{
    for(unsigned long int i = 0; i < num; i++){
        src[i] = dst[i];
    }
}

extern void _start(void);

int main(void)
{

    unsigned long int num_reads = 0, num_writes = 0;

    __asm volatile(
        "addi t0, x0, 5\n           \
         addi t1, x0, 6\n           \
         csrrw x0, mhpmevent3, t0\n \
         csrrw x0, mhpmevent4, t1\n \
         "
         ::: "t0", "t1"
    );

    copy_8bytes((unsigned long int *) _start, buf, 512);

    __asm volatile(
        "csrrs %0, mhpmcounter3, x0\n   \
         csrrs %1, mhpmcounter4, x0\n   \
         "
         : "=r"(num_reads), "=r"(num_writes)
         ::
    );

    printf("Num reads: %lu, num writes: %lu\n", num_reads, num_writes);

    return (num_reads != 0) && (num_writes != 0);
}
