/*
 LLVM Bounds Checking Bounds Checking Library
 CS 6241 Project Phase 1
 Brian Ouellette
 Chad Kersey
 Gaurav Gupta

 Link this with all code that has had the bounds checks inserted. This way,
 intelligent error messages can be produced. The __checkArrayBounds function
 _will_ be inlined by the inliner in places where it would affect performance.
 So don't forget to run the inliner after adding bounds checks.
*/

#define DEBUG

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void __checkArrayBounds(int64_t max, int64_t idx);
static void indexOutOfBounds(int64_t max, int64_t idx);

void __checkArrayBounds(int64_t max, int64_t idx) {
  // Cast the index to an unsigned, because if idx >= 0 and idx < max, then
  // the unsigned version of idx is also < max, and there only has to be one
  // check. Unless DEBUG is defined. Then it might be too big to inline.
  #ifdef DEBUG
  printf("Checking array index %lld vs. max of %lld.\n", 
         (long long)idx, (long long)max);
  #endif
  if ( (uint64_t)idx >= max ) indexOutOfBounds(max, idx);
}

static void indexOutOfBounds(int64_t max, int64_t idx) {
  fprintf(stderr, 
          "Array index %lld exceeds max of %lld. Terminating.\n", 
          (long long)idx, (long long)max
  );

  exit(1);
}
