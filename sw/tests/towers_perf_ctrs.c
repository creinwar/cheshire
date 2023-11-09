// Copyright (c) 2012-2015, The Regents of the University of California (Regents).
// All Rights Reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. Neither the name of the Regents nor the
//    names of its contributors may be used to endorse or promote products
//    derived from this software without specific prior written permission.
// 
// IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
// SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS, ARISING
// OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF REGENTS HAS
// BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
// REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF ANY, PROVIDED
// HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO OBLIGATION TO PROVIDE
// MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

//**************************************************************************
// Towers of Hanoi benchmark
//--------------------------------------------------------------------------
//
// Towers of Hanoi is a classic puzzle problem. The game consists of
// three pegs and a set of discs. Each disc is a different size, and
// initially all of the discs are on the left most peg with the smallest
// disc on top and the largest disc on the bottom. The goal is to move all
// of the discs onto the right most peg. The catch is that you are only
// allowed to move one disc at a time and you can never place a larger
// disc on top of a smaller disc.
//
// This implementation starts with NUM_DISC discs and uses a recursive
// algorithm to sovel the puzzle.

// This is the number of discs in the puzzle.


#include "regs/cheshire.h"
#include "dif/clint.h"
#include "dif/uart.h"
#include "params.h"
#include "util.h"
#include "printf.h"

#define NUM_DISCS  7

extern void __bss_start;
extern void __bss_end;

//--------------------------------------------------------------------------
// List data structure and functions

struct Node
{
  int val;
  struct Node* next;
};

struct List
{
  int size;
  struct Node* head;
};

struct List g_nodeFreeList;
struct Node g_nodePool[NUM_DISCS];

int list_getSize( struct List* list )
{
  return list->size;
}

void list_init( struct List* list )
{
  list->size = 0;
  list->head = 0;
}

void list_push( struct List* list, int val )
{
  struct Node* newNode = NULL;

  // Pop the next free node off the free list
  newNode = g_nodeFreeList.head;
  g_nodeFreeList.head = g_nodeFreeList.head->next;

  // Push the new node onto the given list
  newNode->next = list->head;
  list->head = newNode;

  // Assign the value
  list->head->val = val;

  // Increment size
  list->size++;

}

int list_pop( struct List* list )
{
  struct Node* freedNode = NULL;
  int val = 0;

  // Get the value from the->head of given list
  val = list->head->val;

  // Pop the head node off the given list
  freedNode = list->head;
  list->head = list->head->next;

  // Push the freed node onto the free list
  freedNode->next = g_nodeFreeList.head;
  g_nodeFreeList.head = freedNode;

  // Decrement size
  list->size--;

  return val;
}

void list_clear( struct List* list )
{
  while ( list_getSize(list) > 0 )
    list_pop(list);
}

//--------------------------------------------------------------------------
// Tower data structure and functions

struct Towers
{
  int numDiscs;
  int numMoves;
  struct List pegA;
  struct List pegB;
  struct List pegC;
};

void towers_init( struct Towers* this, int n )
{
  int i = 0;

  this->numDiscs = n;
  this->numMoves = 0;

  list_init( &(this->pegA) );
  list_init( &(this->pegB) );
  list_init( &(this->pegC) );

  for ( i = 0; i < n; i++ )
    list_push( &(this->pegA), n-i );

}

void towers_clear( struct Towers* this )
{

  list_clear( &(this->pegA) );
  list_clear( &(this->pegB) );
  list_clear( &(this->pegC) );

  towers_init( this, this->numDiscs );

}

void towers_solve_h( struct Towers* this, int n,
                     struct List* startPeg,
                     struct List* tempPeg,
                     struct List* destPeg )
{
  int val = 0;

  if ( n == 1 ) {
    val = list_pop(startPeg);
    list_push(destPeg,val);
    this->numMoves++;
  }
  else {
    towers_solve_h( this, n-1, startPeg, destPeg,  tempPeg );
    towers_solve_h( this, 1,   startPeg, tempPeg,  destPeg );
    towers_solve_h( this, n-1, tempPeg,  startPeg, destPeg );
  }

}

void towers_solve( struct Towers* this )
{
  towers_solve_h( this, this->numDiscs, &(this->pegA), &(this->pegB), &(this->pegC) );
}

int towers_verify( struct Towers* this )
{
  struct Node* ptr = NULL;
  int numDiscs = 0;

  if ( list_getSize(&this->pegA) != 0 ) {
    return 2;
  }

  if ( list_getSize(&this->pegB) != 0 ) {
    return 3;
  }

  if ( list_getSize(&this->pegC) != this->numDiscs ) {
    return 4;
  }

  for ( ptr = this->pegC.head; ptr != 0; ptr = ptr->next ) {
    numDiscs++;
    if ( ptr->val != numDiscs ) {
      return 5;
    }
  }

  if ( this->numMoves != ((1 << this->numDiscs) - 1) ) {
    return 6;
  }

  return 0;
}

//--------------------------------------------------------------------------
// Main

int tower_test(void)
{
  struct Towers towers = {0};
  int i = 0;

  // Initialize free list

  list_init( &g_nodeFreeList );
  g_nodeFreeList.head = &(g_nodePool[0]);
  g_nodeFreeList.size = NUM_DISCS;
  g_nodePool[NUM_DISCS-1].next = 0;
  g_nodePool[NUM_DISCS-1].val = 99;
  for ( i = 0; i < (NUM_DISCS-1); i++ ) {
    g_nodePool[i].next = &(g_nodePool[i+1]);
    g_nodePool[i].val = i;
  }

  towers_init( &towers, NUM_DISCS );

  towers_solve( &towers );

  // Solve it

  towers_clear( &towers );
  //setStats(1);
  towers_solve( &towers );
  //setStats(0);

  // Check the results
  return towers_verify( &towers );
}

int main(void)
{
  unsigned long int perf_ctr_1 = 0;
  unsigned long int perf_ctr_2 = 0;
  unsigned long int perf_ctr_3 = 0;
  unsigned long int perf_ctr_4 = 0;
  unsigned long int perf_ctr_5 = 0;
  unsigned long int perf_ctr_6 = 0;

  uint32_t rtc_freq = *reg32(&__base_regs, CHESHIRE_RTC_FREQ_REG_OFFSET);
  uint64_t reset_freq = clint_get_core_freq(rtc_freq, 2500);
  uart_init(&__base_uart, reset_freq, 115200);

  // 6 registers, 22 actual perf counters = 4 iterations
  for(int i = 0; i < 4; i++){

    __asm volatile(
        "add t0, x0, %0\n             \
         csrrw x0, mhpmevent3, t0\n   \
         addi t0, t0, 1\n             \
         csrrw x0, mhpmcounter3, x0\n \
         csrrw x0, mhpmevent4, t0\n   \
         addi t0, t0, 1\n             \
         csrrw x0, mhpmcounter4, x0\n \
         csrrw x0, mhpmevent5, t0\n   \
         addi t0, t0, 1\n             \
         csrrw x0, mhpmcounter5, x0\n \
         csrrw x0, mhpmevent6, t0\n   \
         addi t0, t0, 1\n             \
         csrrw x0, mhpmcounter6, x0\n \
         csrrw x0, mhpmevent7, t0\n   \
         addi t0, t0, 1\n             \
         csrrw x0, mhpmcounter7, x0\n \
         csrrw x0, mhpmevent8, t0\n   \
         csrrw x0, mhpmcounter8, x0\n \
         .word 0xfffff00b\n           \
         "
         :: "r"((i*6)+1)
         : "t0"
    );

    tower_test();

    __asm volatile(
        "csrrs %0, mhpmcounter3, x0\n   \
         csrrs %1, mhpmcounter4, x0\n   \
         csrrs %2, mhpmcounter5, x0\n   \
         csrrs %3, mhpmcounter6, x0\n   \
         csrrs %4, mhpmcounter7, x0\n   \
         csrrs %5, mhpmcounter8, x0\n   \
         "
         : "=r"(perf_ctr_1), "=r"(perf_ctr_2), "=r"(perf_ctr_3), "=r"(perf_ctr_4), "=r"(perf_ctr_5), "=r"(perf_ctr_6)
         ::
    );

    printf("ctr %d: %lu\n", i*6+1, perf_ctr_1);
    printf("ctr %d: %lu\n", i*6+2, perf_ctr_2);
    printf("ctr %d: %lu\n", i*6+3, perf_ctr_3);
    printf("ctr %d: %lu\n", i*6+4, perf_ctr_4);
    printf("ctr %d: %lu\n", i*6+5, perf_ctr_5);
    printf("ctr %d: %lu\n", i*6+6, perf_ctr_6);
  }
}

