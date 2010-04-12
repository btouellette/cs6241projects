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
        (*it)->eraseFromParent();
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
        uint64_t ub = consUB->getSExtValue();
        
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
      // Set of instructions to delete which is returned to the callee
      set<Instruction*> insToDel;
      // All instructions we've checked so far (this will be searched for
      // duplicates)
      map<Value*, Value*> checkedIns;
      map<Value*, Value*>::iterator it;
      // Iterate over every instruction in this BasicBlock
      for(BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I) {
        // Only start checking when we find an upper bounds check
        if (!I->getName().str().compare(0, 12, "_arrayref ub")) {
          // Go ahead and pull out the references in the upper bounds and index
          // checks
          Value *ub = I->getOperand(0);
          Value *idx = (++I)->getOperand(0);
          // Booleans used to see if both the upper bounds and the index is
          // identical to anything already in our checked set
          bool dupe = false;
          bool dupeUB = false;
          bool dupeIDX = false;
          // Iterate over all the instructions we've previously added to the
          // checked set
          for (it=checkedIns.begin(); it != checkedIns.end(); it++) {
            dupeUB = dupeIDX = false;
            // Retrieves the stored upper bounds and index values (either an
            // Instruction or a ConstantInt
            Value *ub2 = (*it).first;
            Value *idx2 = (*it).second;
            // If both references are Instructions just check the pointer to see
            // if they are the same. Since the IR is SSA this check is
            // sufficient.
            if (isa<Instruction>(*ub) && isa<Instruction>(*ub2)) {
              if (ub == ub2) {
                dupeUB = true;
              }
            }
            // If they are both ConstantInts then we need to pull out the value
            // and check them against each other
            else if (!isa<Instruction>(*ub) && !isa<Instruction>(*ub2)) {
              // Pull out the upper bound LLVM representation
              ConstantInt *consUB = dynamic_cast<ConstantInt*>(ub);
              // Get the sign extended value (for zero extended use ZExt)
              uint64_t ub = consUB->getSExtValue();
              
              // Pull out the upper bound LLVM representation
              ConstantInt *consUB2 = dynamic_cast<ConstantInt*>(ub2);
              // Get the sign extended value (for zero extended use ZExt)
              uint64_t ub2 = consUB2->getSExtValue();

              if (ub == ub2) {
                dupeUB = true;
              }
            }
            // If both references are Instructions just check the pointer to see
            // if they are the same. Since the IR is SSA this check is
            // sufficient.
            if (isa<Instruction>(*idx) && isa<Instruction>(*idx2)) {
              if (idx == idx2) {
                dupeIDX = true;
              }
            }
            // If they are both ConstantInts then we need to pull out the value
            // and check them against each other
            else if (!isa<Instruction>(*idx) && !isa<Instruction>(*idx2)) {
              // Pull out the upper bound LLVM representation
              ConstantInt *consIDX = dynamic_cast<ConstantInt*>(idx);
              // Get the sign extended value (for zero extended use ZExt)
              uint64_t idx = consIDX->getSExtValue();
              
              // Pull out the upper bound LLVM representation
              ConstantInt *consIDX2 = dynamic_cast<ConstantInt*>(idx2);
              // Get the sign extended value (for zero extended use ZExt)
              uint64_t idx2 = consIDX2->getSExtValue();

              if (idx == idx2) {
                dupeIDX = true;
              }
            }
            // If the upper bounds and the index are identical to the
            // Instruction we are looking at then it is a duplicate check.
            if (dupeUB && dupeIDX) {
              dupe = true;
            }
          }
          // If it isn't a dupe add it to the set of checked Instructions
          if (!dupe) {
            checkedIns.insert(pair<Value*, Value*>(ub, idx));
          }
          // If it is a dupe add both checks to the list of Instructions to
          // delete
          else {
            insToDel.insert(&(*(--I)));
            errs() << "Removed" << *I << "\n";
            insToDel.insert(&(*(++I)));
            errs() << "Removed" << *I << "\n";
          }
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