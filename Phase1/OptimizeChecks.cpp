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
#include <map>

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
      for(Function::iterator BB = F.begin(), BB_E = F.end(); BB != BB_E; ++BB) {
        // Check for local redundant checks
        set<Instruction*> newInsToDel = eliminateRedundantLocalChecks(&(*BB));
        // Add any redundant checks to the set to be deleted later
        insToDel.insert(newInsToDel.begin(), newInsToDel.end());
        for(BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I) {
          // Check to see if this instruction is an upper bounds check
          if (!I->getName().str().compare(0, 12, "_arrayref ub")) {
            // Eliminate checks we can statically determine
            Instruction *Iub = &(*I);
            Instruction *Iidx = &(*(++I));
            if(staticallyDetermineCheck(Iub, Iidx)) {
              // It fits so flag instructions for deletion
              insToDel.insert(&(*(--I)));
              errs() << "Removed" << *I << "\n";
              insToDel.insert(&(*(++I)));
              errs() << "Removed" << *I << "\n";
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

    bool staticallyDetermineCheck(Instruction *Iub, Instruction *Iidx)
    {
      //errs() << "Checking" << *Iub << "\n";
      //errs() << "Checking" << *Iidx << "\n";
      // Verify that the operand of the current instruction is a constant
      Value *op = Iub->getOperand(0);
      if (!isa<Instruction>(*op)) {
        // Pull out the upper bound LLVM representation
        ConstantInt *consUB = dynamic_cast<ConstantInt*>(op);
        // Get the sign extended value (for zero extended use ZExt)
        int64_t ub = consUB->getSExtValue();
        
        // Check the following index to see if it is a constant
        op = Iidx->getOperand(0);
        if (!isa<Instruction>(*op)) {
          ConstantInt *consIDX = dynamic_cast<ConstantInt*>(op);
          uint64_t idx = consIDX->getSExtValue();
          
          // Statically check index fits in upper bound
          if (idx <= ub) {
            return true;
          }
        }
      }
      return false;
    }
    
    set<Instruction*> eliminateRedundantLocalChecks(BasicBlock *BB)
    {
      set<Instruction*> insToDel;
      map<Value*, Value*> checkedIns;
      map<Value*, Value*>::iterator it;
      for(BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I) {
          if (!I->getName().str().compare(0, 12, "_arrayref ub")) {
            Value *ub = I->getOperand(0);
            Value *idx = (++I)->getOperand(0);
            checkedIns.insert(pair<Value*, Value*>(ub, idx));
          }
      }
      return insToDel;
    }
      
    //Add required analyses here
    void getAnalysisUsage(AnalysisUsage &AU) const
    {

    }
  };

  char OptimizeChecks::ID = 0;
  RegisterPass<OptimizeChecks> X("OptimizeChecks", "Redundant check removal");
}
