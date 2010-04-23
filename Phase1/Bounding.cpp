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
    }

    virtual bool runOnLoop(Loop *L, LPPassManager &LPM) {
      typedef Loop::block_iterator l_it;
      typedef BasicBlock::iterator b_it;
      typedef Value::use_iterator u_it;

      bool modified = false;

      if (L->getLoopPreheader() == NULL) return false;

      errs() << "LOOP\n";
      // Iterate over basic blocks in the loop. Looking for array references
      // using induction variables, which will be held in memory, because no
      // pointer analysis can have happened yet.
      for (l_it B = L->block_begin(); B != L->block_end(); B++) {
        for (b_it I = (*B)->begin(); I != (*B)->end(); I++) {
          if (I->getName().str().compare(0, 12, "_arrayref ub")) continue;
          
          Value *ub = I->getOperand(0);
          Value *idx = (++I)->getOperand(0);
            
          // Get the pointer to the memory location
          LoadInst *LI = dyn_cast<LoadInst>(idx);
          if (LI == NULL) continue;
          Value *P = LI->getPointerOperand();

          // Must have the following uses:
          //   - An original store, in the loop preheader: the lower bound.
          //   - A load within the loop for an increment.
          //   - A store within the loop for an increment.
          //   - A load within the loop header for the array bound.
          // Must not have any further stores.
          if (!(P->hasNUsesOrMore(2))) continue;
          
          StoreInst *lbStore = NULL, *incStore = NULL;
          Value *loop_lb = NULL, *loop_ub = NULL;
          ICmpInst *cmpInst;
          LoadInst *incLoad = NULL, *cmpLoad = NULL;

          // Try to find the store and value for the loop lower bound.
          for (b_it I = (L->getLoopPreheader())->begin(); 
               I != (L->getLoopPreheader())->end() && lbStore == NULL; 
               I++) {
            for (u_it J = P->use_begin(); J != P->use_end(); J++) {
              if (dyn_cast<Instruction>(I) == dyn_cast<Instruction>(J) && 
                  (lbStore = dyn_cast<StoreInst>(I)) ) {
                loop_lb = lbStore->getOperand(0);
              }
            }
          }

          if (loop_lb == NULL) continue;

          // Try to find the load and value for the upper bound compare.
          for (b_it I = (L->getHeader())->begin();
               I != (L->getHeader())->end();
               I++) {
            for (u_it J = P->use_begin(); J != P->use_end(); J++) {
              if (dyn_cast<Instruction>(I) == dyn_cast<Instruction>(J) &&
                  (cmpLoad = dyn_cast<LoadInst>(I)) &&
                  cmpLoad->hasNUses(1) &&
                  (cmpInst = dyn_cast<ICmpInst>(*(cmpLoad->use_begin()))) &&
                  cmpInst->getPredicate() == CmpInst::ICMP_SGT) {
                loop_ub = cmpInst->getOperand(0);
              }
            }
          }
          
          if (loop_ub == NULL) continue;

          errs() << "UB: " << *loop_ub << '\n' << "LB: " << *loop_lb << '\n';

          // Try to find the increment, for verification.

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
  RegisterPass<BoundChecks> X("BoundChecks", "Hoist checks from bounded loops");
}
