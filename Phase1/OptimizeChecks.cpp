/*
 LLVM Bounds Checking Optimization Pass
 CS 6241 Project Phase 1
 Brian Ouellette
 Chad Kersey
 Gaurav Gupta
*/

#define DEBUG_TYPE "OptimizeChecks"

#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#include <stdint.h>
#include <set>

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/InstIterator.h"

#include "llvm/ADT/Statistic.h"

using namespace llvm;
using namespace std;

namespace
{
  STATISTIC(numChecksDeleted, "Number of bounds checks deleted.");

  struct OptimizeChecks : public FunctionPass
  {
  public:
    static char ID;
    OptimizeChecks() : FunctionPass(&ID) {}

    virtual bool doInitialization(Module &M) 
    {
      numChecksDeleted = 0;
    }

    virtual bool runOnFunction(Function &F)
    {
      set<Instruction*> insToDel;
      // Iterate over all instructions in the function 
      for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
        // Eliminate checks we can statically determine
        // Check to see if this instruction is an upper bounds check
        if (!I->getName().str().compare(0, 12, "_arrayref ub")) {
          // Verify that the operand of the current instruction is a constant
          Value *op = I->getOperand(0);
          if (!isa<Instruction>(*op)) {
            // Pull out the upper bound LLVM representation
            ConstantInt *consUB = dynamic_cast<ConstantInt*>(op);
            // Get the sign extended value (for zero extended use ZExt)
            int64_t ub = consUB->getSExtValue();
            
            // Check the following index to see if it is a constant
            op = (++I)->getOperand(0);
            if (!isa<Instruction>(*op)) {
              ConstantInt *consIDX = dynamic_cast<ConstantInt*>(op);
              int64_t idx = consIDX->getSExtValue();
              
              // Statically check index fits in upper bound
              if (idx <= ub) {
                // It fits so flag instructions for deletion
                insToDel.insert(&(*(--I)));
                errs() << "Removed" << *I << "\n";
                insToDel.insert(&(*(++I)));
                errs() << "Removed" << *I << "\n";
              }
            }
          }
        }
      }
      
      set<Instruction*>::iterator it;
      for (it = insToDel.begin(); it != insToDel.end(); ++it) {
        (*it)->removeFromParent();
        numChecksDeleted++;
      }
      //Possibly modified function so return true
      return true;
    }
      
    //Add required analyses here
    void getAnalysisUsage(AnalysisUsage &AU) const
    {

    }
  };

  char OptimizeChecks::ID = 0;
  RegisterPass<OptimizeChecks> X("OptimizeChecks", "Redundant check removal");
}
