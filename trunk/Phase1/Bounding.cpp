/*
 LLVM Bounds Checking Optimization Pass for Part 3
 CS 6241 Project Phase 1
 Brian Ouellette
 Chad Kersey
 Gaurav Gupta
*/

#define DEBUG_TYPE "BoundChecks"

#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#include <stdint.h>
#include <set>
#include <map>

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/CFG.h"

#include "llvm/ADT/Statistic.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"

using namespace llvm;
using namespace std;

typedef pair<Instruction*,Instruction*> inspair;

namespace
{
  STATISTIC(numChecksMoved, "Number of bounds checks moved.");

  struct BoundChecks : public LoopPass
  {
  public:
    static char ID;
    BoundChecks() : LoopPass(&ID) {}

    virtual void getAnalysisUsage(AnalysisUsage &AU) {
      AU.addRequired<>();
    }

    virtual bool runOnLoop(Loop *L, LPPassManager &LPM) {
      typedef Loop::block_iterator l_it;
      typedef BasicBlock::iterator b_it;
      

      bool modified = false;

      Value *ind_var = L->getCanonicalInductionVariable();
      Value *max_var = L->getTripCount();

      errs() << "LOOP: ";
      if (ind_var) errs() << (*ind_var); else errs() << " - ";
      if (max_var) errs() << (*max_var); else errs() << " - \n";

      // Iterate over basic blocks in the loop. Looking for array references
      // using induction variables.
      for (l_it B = L->block_begin(); B != L->block_end(); B++) {
        for (b_it I = (*B)->begin(); I != (*B)->end(); I++) {
          errs() << ".\n";

          if (!I->getName().str().compare(0, 12, "_arrayref ub")) {
            Value *ub = I->getOperand(0);
            Value *idx = (++I)->getOperand(0);
            
            errs() << "ub: " << *ub << "\nidx: " << *idx << "\n";
          }
        }
      }

      return modified;
    }

    //Add required analyses here
    void getAnalysisUsage(AnalysisUsage &AU) const
    {

    }
  };

  char BoundChecks::ID = 0;
  RegisterPass<BoundChecks> X("BoundChecks", "Redundant check removal");
}
