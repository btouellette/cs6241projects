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

#include <stdint.h>
#include <cstdio>
#include <cstdlib>

#include <map>

#ifdef DEBUG
// Access frequencies at each array access address.
std::map<void*, uint64_t> freq;
typedef std::map<void*, uint64_t>::iterator freq_it;
uint64_t n_checks = 0;
#endif

extern "C" {
void __checkArrayBounds(int64_t max, int64_t idx);
static void indexOutOfBounds(int64_t max, int64_t idx);

#ifdef DEBUG
__attribute ((destructor)) void __printArrayCheckStats(void) 
{
  for (freq_it i = freq.begin(); i != freq.end(); i++) {
    printf("%09llu (%p)\n", (unsigned long long)((*i).second), i->first);
  }
  printf("%llu total checks\n", (unsigned long long)n_checks);
}

// If we're profiling, this function shouldn't be inlined, since the return
// address is a valuable statistic.
__attribute__ ((noinline))
#endif
void __checkArrayBounds(int64_t max, int64_t idx)
{
  // Cast the index to an unsigned, because if idx >= 0 and idx < max, then
  // the unsigned version of idx is also < max, and there only has to be one
  // check. Unless DEBUG is defined. Then it might be too big to inline.
  #ifdef DEBUG
  void *ra = __builtin_return_address(0);
  if (freq.find(ra) == freq_it(NULL)) freq[ra] = 1;
  else freq[ra]++;
  n_checks++;
  #endif

  if ( (uint64_t)idx > max ) indexOutOfBounds(max, idx);
}

static void indexOutOfBounds(int64_t max, int64_t idx) {
  fprintf(stderr, 
          "Array index %lld exceeds max of %lld. Terminating.\n", 
          (long long)idx, (long long)max
  );

  exit(1);
}
};
