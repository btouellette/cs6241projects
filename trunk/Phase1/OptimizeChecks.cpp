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
    OptimizeChecks() : FunctionPass(&ID)
    {
    }

    virtual bool
    doInitialization(Module &M)
    {
      numChecksDeleted = 0;
    }

    //Monotonic type
    enum monotonic
    {
      unchanged, increment, decrement, multiply, divg1, divl1, changed
    };
    //divg1 means div > 1, divl1 means div < 1

    //TODO: Implement affect
    //Analyzes a particular operand in a basic block to check its monotonic properties
    //Implementation: Retrieve each occurence of the operand idx in the basic block and
    //check if there is a constant increase/decrease/multiplication
    monotonic
    affect(BasicBlock *B, Value *idx)
    {

    }
    //Calculates the gen set for availability
    &set<Instruction *> C_GEN(BasicBlock *B)
    {
      set<Instruction*> *cg = new set<Instruction*>();
      set<Instruction*> &cgen = *cg;
      for(BasicBlock::iterator I = B->begin(), E = B->end(); I != E; ++I)
      {
        if (!I->getName().str().compare(0, 12, "_arrayref ub") || !I->getName().str().compare(0, 12, "_arrayref lb"))
        {
          cgen.insert(&(*(--I)));
          cgen.insert(&(*(++I)));
        }
      }
      return cgen;
    }

    //Calculates the forward set for a particular basic block
    //Input: C_IN, set of checks available on entry to basic block.
    //Output: set of forward propogation of checks
    //Important Note - this does not cover f(v) i.e. function analysis.
    //This means that it might miss some cases.
    &set<Instruction *> forward(&set<Instruction *> C_IN,BasicBlock *B)
    {
      set<Instruction*> *fc = new set<Instruction*>();
      set<Instruction*> &fwdChecks = *fc;
      for(set<Instruction *>::iterator I = C_IN->begin(), E = C_IN->end(); I != E; ++I)
      {
        //get lowerbound and the index from valid checks
        if (!I->getName().str().compare(0, 12, "_arrayref lb"))
        {
          Value *lbv = I->getOperand(0);
          Value *idx = (++I)->getOperand(0);
          if (!isa<Instruction>(*lbv))
          {
            ConstantInt *consLB = dynamic_cast<ConstantInt*>(lbv);
            uint64_t lb = consLB->getSExtValue();
            if(lb<=idx)
            {
              monotonic result = affect(B, idx);
              switch(result)
              {
                case unchanged:
                case increment:
                case multiply:
                case divl1:
                fwdChecks.insert(&(*(--I)));
                fwdChecks.insert(&(*(++I)));
              }
            }
          }
        }
        //get upperbound and the index from valid checks
        if (!I->getName().str().compare(0, 12, "_arrayref ub"))
        {
          Value *ubv = I->getOperand(0);
          Value *idx = (++I)->getOperand(0);
          if (!isa<Instruction>(*ubv))
          {
            ConstantInt *consLB = dynamic_cast<ConstantInt*>(ubv);
            uint64_t ub = consLB->getSExtValue();
            if(ub>=idx)
            {
              monotonic result = affect(B, idx);
              switch(result)
              {
                case unchanged:
                case decrement:
                case divg1:
                fwdChecks.insert(&(*(--I)));
                fwdChecks.insert(&(*(++I)));
              }
            }
          }
        }
      }
      return fwdChecks;
    }

    //Calculates the out set for availability
    &set<Instruction *> C_OUT(&set<Instruction *> C_IN, BasicBlock *B)
    {
      set<Instruction*> *co = new set<Instruction*>();
      set<Instruction*> &cout = *co;
      set<Instruction*> &fwdChecks = forward(C_IN, B);
      set<Instruction*> &cgen = C_GEN(B);
      for(set<Instruction *>::iterator I = fwdChecks->begin(), E = fwdChecks->end(); I != E; ++I)
        cout.insert(&(*I));
      for(set<Instruction *>::iterator I = cgen->begin(), E = cgen->end(); I != E; ++I)
        cout.insert(&(*I));
      return cout;
    }
    //Implements C_IN
    //Returns an empty set for the first Basic Block
    //TODO: Implement c_in
    &set<Instruction *> C_IN(BasicBlock *B)
    {
      set<Instruction*> *ci = new set<Instruction*>();
      set<Instruction*> &cin = *ci;
      return cin;
    }

    virtual bool runOnFunction(Function &F)
    {
      set<Instruction*> insToDel;
      // Iterate over all instructions in the function
      for(Function::iterator BB = F.begin(), BB_E = F.end(); BB != BB_E; ++BB)
      {
        // Check for local redundant checks
        set<Instruction*> newInsToDel = eliminateRedundantLocalChecks(&(*BB));
        // Add any redundant checks to the set to be deleted later
        insToDel.insert(newInsToDel.begin(), newInsToDel.end());
        for(BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I)
        {
          // Check to see if this instruction is an upper bounds check
          if (!I->getName().str().compare(0, 12, "_arrayref ub"))
          {
            // Eliminate checks we can statically determine
            Instruction *Iub = &(*I);
            Instruction *Iidx = &(*(++I));
            if(staticallyDetermineCheck(Iub, Iidx))
            {
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
      for (it = insToDel.begin(); it != insToDel.end(); ++it)
      {
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
      if (!isa<Instruction>(*op))
      {
        // Pull out the upper bound LLVM representation
        ConstantInt *consUB = dynamic_cast<ConstantInt*>(op);
        // Get the sign extended value (for zero extended use ZExt)
        uint64_t ub = consUB->getSExtValue();

        // Check the following index to see if it is a constant
        op = Iidx->getOperand(0);
        if (!isa<Instruction>(*op))
        {
          ConstantInt *consIDX = dynamic_cast<ConstantInt*>(op);
          uint64_t idx = consIDX->getSExtValue();

          // Statically check index fits in upper bound
          if (idx <= ub)
          {
            return true;
          }
        }
      }
      return false;
    }

    //TODO: Fix: Returning a local object instead of a reference to a "new" object
    set<Instruction*> eliminateRedundantLocalChecks(BasicBlock *BB)
    {
      // Set of instructions to delete which is returned to the callee
      set<Instruction*> insToDel;
      // All instructions we've checked so far (this will be searched for
      // duplicates)
      map<Value*, Value*> checkedIns;
      map<Value*, Value*>::iterator it;
      // Iterate over every instruction in this BasicBlock
      for(BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I)
      {
        // Only start checking when we find an upper bounds check
        if (!I->getName().str().compare(0, 12, "_arrayref ub"))
        {
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
          for (it=checkedIns.begin(); it != checkedIns.end(); it++)
          {
            dupeUB = dupeIDX = false;
            // Retrieves the stored upper bounds and index values (either an
            // Instruction or a ConstantInt
            Value *ub2 = (*it).first;
            Value *idx2 = (*it).second;
            // If both references are Instructions just check the pointer to see
            // if they are the same. Since the IR is SSA this check is
            // sufficient.
            if (isa<Instruction>(*ub) && isa<Instruction>(*ub2))
            {
              if (ub == ub2)
              {
                dupeUB = true;
              }
            }
            // If they are both ConstantInts then we need to pull out the value
            // and check them against each other

            else if (!isa<Instruction>(*ub) && !isa<Instruction>(*ub2))
            {
              // Pull out the upper bound LLVM representation
              ConstantInt *consUB = dynamic_cast<ConstantInt*>(ub);
              // Get the sign extended value (for zero extended use ZExt)
              uint64_t ub = consUB->getSExtValue();

              // Pull out the upper bound LLVM representation
              ConstantInt *consUB2 = dynamic_cast<ConstantInt*>(ub2);
              // Get the sign extended value (for zero extended use ZExt)
              uint64_t ub2 = consUB2->getSExtValue();

              if (ub == ub2)
              {
                dupeUB = true;
              }
            }
            // If both references are Instructions just check the pointer to see
            // if they are the same. Since the IR is SSA this check is
            // sufficient.
            if (isa<Instruction>(*idx) && isa<Instruction>(*idx2))
            {
              if (idx == idx2)
              {
                dupeIDX = true;
              }
            }
            // If they are both ConstantInts then we need to pull out the value
            // and check them against each other

            else if (!isa<Instruction>(*idx) && !isa<Instruction>(*idx2))
            {
              // Pull out the upper bound LLVM representation
              ConstantInt *consIDX = dynamic_cast<ConstantInt*>(idx);
              // Get the sign extended value (for zero extended use ZExt)
              uint64_t idx = consIDX->getSExtValue();

              // Pull out the upper bound LLVM representation
              ConstantInt *consIDX2 = dynamic_cast<ConstantInt*>(idx2);
              // Get the sign extended value (for zero extended use ZExt)
              uint64_t idx2 = consIDX2->getSExtValue();

              if (idx == idx2)
              {
                dupeIDX = true;
              }
            }
            // If the upper bounds and the index are identical to the
            // Instruction we are looking at then it is a duplicate check.
            if (dupeUB && dupeIDX)
            {
              dupe = true;
            }
          }
          // If it isn't a dupe add it to the set of checked Instructions
          if (!dupe)
          {
            checkedIns.insert(pair<Value*, Value*>(ub, idx));
          }
          // If it is a dupe add both checks to the list of Instructions to
          // delete
          else
          {
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
