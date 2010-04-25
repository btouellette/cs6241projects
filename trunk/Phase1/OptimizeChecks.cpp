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
#include "llvm/Support/CFG.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Analysis/LoopInfo.h"

#include "llvm/ADT/Statistic.h"

using namespace llvm;
using namespace std;

typedef pair<Instruction*,Instruction*> inspair;

namespace
{
  STATISTIC(numChecksHoisted, "Number of bounds checks hoisted.");
  STATISTIC(numChecksPropagated, "Number of bounds checks propagated.");
  STATISTIC(numChecksDeleted, "Number of bounds checks deleted.");

  struct OptimizeChecks : public FunctionPass
  {
  public:
    static char ID;
    OptimizeChecks() : FunctionPass(&ID) {}

    virtual bool doInitialization(Module &M) 
    {
      numChecksHoisted = 0;
      numChecksPropagated = 0;
      numChecksDeleted = 0;
    }

    virtual bool runOnFunction(Function &F)
    {
      // Propagate checks out of loops if possible, repeating until no change
      while(loopPropagation(&F));

      map< BasicBlock*,set<inspair> > IN;
      map< BasicBlock*,set<inspair> > OUT;
      map< BasicBlock*,set<inspair> > KILL;
      map< BasicBlock*,set<inspair> > GEN;

      set<Instruction*> insToDel; 

      // Iterate over all BasicBlocks in the function 
      for(Function::iterator BB = F.begin(), BB_E = F.end(); BB != BB_E; ++BB) {
        // Initialize empty sets for this BB
        IN.insert(make_pair(&(*BB), set<inspair>()));
        OUT.insert(make_pair(&(*BB), set<inspair>()));
        KILL.insert(make_pair(&(*BB), set<inspair>()));
        GEN.insert(make_pair(&(*BB), set<inspair>()));
        // Check for local redundant checks
        set<Instruction*> newInsToDel = eliminateRedundantLocalChecks(&(*BB));
        // Add any redundant checks to the set to be deleted later
        insToDel.insert(newInsToDel.begin(), newInsToDel.end());
        for(BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I) {
          // Check to see if this instruction is an upper bounds check
          if (!I->getName().str().compare(0, 12, "_arrayref ub")) {
            Instruction *Iub = &(*I);
            Instruction *Iidx = &(*(++I));

            // Eliminate checks we can statically determine
            if(staticallyDetermineCheck(Iub, Iidx)) {
              // It fits so flag instructions for deletion
              insToDel.insert(&(*(--I)));
              //errs() << "STATIC Removed" << *I << "\n";
              insToDel.insert(&(*(++I)));
              //errs() << "STATIC Removed" << *I << "\n";
            }
            // If it isn't going to be erased add it to GEN and OUT for this BB
            else {
              GEN[&(*BB)].insert(make_pair(Iub, Iidx));
              OUT[&(*BB)].insert(make_pair(Iub, Iidx));
            }
          }
        }
      }
      
      set<Instruction*> newInsToDel = globalElimination(&F,IN,OUT,GEN,KILL);
      // Add any redundant checks to the set to be deleted later
      insToDel.insert(newInsToDel.begin(), newInsToDel.end());

      set<Instruction*>::iterator it;
      for (it = insToDel.begin(); it != insToDel.end(); ++it) {
        (*it)->eraseFromParent();
        numChecksDeleted++;
      }
      numChecksDeleted = numChecksDeleted/2;
      //Possibly modified function so return true
      return true;
    }

    bool loopPropagation(Function *F)
    {
      int oldHoisted = numChecksHoisted;
      int oldPropagated = numChecksPropagated;

      DominatorTree *DT = &getAnalysis<DominatorTree>();
      LoopInfo *LI = &getAnalysis<LoopInfo>();

      // Iterates over all the top level loops in the function
      for(LoopInfo::iterator I = LI->begin(), E = LI->end(); I != E; ++I) {
        // Step 1: Determine the blocks from which checks can be propagated out
        // of loops
        Loop *L = *I;
        
        // Get the set of exiting blocks from the current loop
        SmallVector<BasicBlock*, 20> exits;
        L->getExitingBlocks(exits);

        set<BasicBlock*> loopDomBlocks; 
        vector<BasicBlock*> loopBlocks = L->getBlocks();
        
        bool valid = true;
        // Iterate over all the BBs in the current loop
        for(int k = 0; k < loopBlocks.size(); k++) {
          BasicBlock *BB = loopBlocks[k];
          // Check whether the current BB doms all exits
          for(int j = 0; j < exits.size(); j++) {
            BasicBlock *BB_exit = exits[j];
            if(!DT->dominates(BB,BB_exit)) {
              valid = false;
            }
          }
          // If the current BB doms all loop exits
          if(valid) {
            loopDomBlocks.insert(BB);
          }
          valid = true;
        }
        // Step 2: Propagate checks from nodes that do not dominate all loop exits
        // to nodes that dominate all loop exits
        //
        // Calculate set P of valid propagation targets
        set<BasicBlock*> P;
        for(int k = 0; k < loopBlocks.size(); k++) {
          BasicBlock *BB = loopBlocks[k];
          bool unique = true;
          bool valid = false;
          for(succ_iterator SI = succ_begin(BB), SE = succ_end(BB); SI != SE; ++SI) {
            BasicBlock *succ = *SI;
            // Ensure that all successors of the BB have the BB as their unique
            // predecessor
            if(succ->getUniquePredecessor() != BB) {
              unique = false;
            }
            // Ensure that at least one successor of the BB is in the set of
            // nodes which do not dominate the loop exits
            if(loopDomBlocks.count(succ) == 0) {
              valid = true;
            }
          }
          if(unique && valid) {
            P.insert(BB);
            //errs() << *BB << "\n";
          }
        }

        // Now actually perform the propagations
        bool change = true;
        while(change) {
          change = false;
          set<BasicBlock*>::iterator it;
          // We only propagate checks into BasicBlocks in P
          for(it = P.begin(); it != P.end(); ++it) {
            BasicBlock *BB = *it;
            // Find intersection of checks performed in all successors
            set<inspair> intersect;
            map< inspair, set<inspair> > intersectMap;
            bool firstRun = true;
            for(succ_iterator SI = succ_begin(BB), SE = succ_end(BB); SI != SE; ++SI) {
              //errs() << "-----------------------------------\n";
              BasicBlock *succ = *SI;
              //errs() << *succ << "\n";
              // On the first block we initialize the intersection set to all
              // checks in this BB
              if(SI == succ_begin(BB)) {
                // Check all instructions in the successor
                for(BasicBlock::iterator I = succ->begin(), E = succ->end(); I != E; ++I) {
                  // Check to see if this instruction is an upper bounds check
                  if(!I->getName().str().compare(0, 12, "_arrayref ub")) {
                    Instruction *Iub = &(*I);
                    Instruction *Iidx = &(*(++I));
                    intersect.insert(make_pair(Iub, Iidx));
                    //errs() << *Iub << "\n";
                    //errs() << *Iidx << "\n";
                  }
                }
              }
              // Ohterwise we remove anything from the intersect set that is not
              // in the current BB
              else {
                set<inspair> current;
                // Check all instructions in the successor
                for(BasicBlock::iterator I = succ->begin(), E = succ->end(); I != E; ++I) {
                  // Check to see if this instruction is an upper bounds check
                  if(!I->getName().str().compare(0, 12, "_arrayref ub")) {
                    Instruction *Iub = &(*I);
                    Instruction *Iidx = &(*(++I));
                    current.insert(make_pair(Iub, Iidx));
                  }
                }
                // Iterate over everything currently in the intersection set
                set<inspair>::iterator it2;
                for(it2 = intersect.begin(); it2 != intersect.end(); ++it2) {
                  inspair intersectPair = *it2;
                  bool valid = false;
                  // Iterate over everything in the current successor looking
                  // for something the same as the current item in the
                  // intersection set
                  set<inspair>::iterator it3;
                  for(it3 = current.begin(); it3 != current.end(); ++it3) {
                    inspair currentPair = *it3;
                    // Check that the upper bounds are identical
                    bool ubValid = false;
                    Instruction *Iub1 = intersectPair.first;
                    Instruction *Iub2 = currentPair.first;
                    // Pull out the actual values of the upper bound and index
                    Value *ub1 = Iub1->getOperand(0);
                    Value *ub2 = Iub2->getOperand(0);
                    // If they are instructions just compare the pointers
                    if(isa<Instruction>(*ub1) && isa<Instruction>(*ub2) && ub1 == ub2) {
                      ubValid = true;
                    }
                    // Otherwise we need to compare the actual values of the
                    // constants
                    else if(isa<ConstantInt>(*ub1) && isa<ConstantInt>(*ub2)) {
                      ConstantInt *consUB1 = dynamic_cast<ConstantInt*>(ub1);
                      ConstantInt *consUB2 = dynamic_cast<ConstantInt*>(ub2);
                      // Get the sign extended value (for zero extended use ZExt)
                      int64_t intUB1 = consUB1->getSExtValue();
                      int64_t intUB2 = consUB2->getSExtValue();
                      if(intUB1 == intUB2) {
                        ubValid = true;
                      }
                    }
                    // Now check that the indices are identical
                    bool idxValid = false;
                    Instruction *Iidx1 = intersectPair.second;
                    Instruction *Iidx2 = currentPair.second;
                    // Pull out the actual values of the upper bound and index
                    Value *idx1 = Iidx1->getOperand(0);
                    Value *idx2 = Iidx2->getOperand(0);
                    // If they are instructions just compare the pointers
                    if(isa<Instruction>(*idx1) && isa<Instruction>(*idx2) && idx1 == idx2) {
                      idxValid = true;
                    }
                    // Otherwise we need to compare the actual values of the
                    // constants
                    else if(isa<ConstantInt>(*idx1) && isa<ConstantInt>(*idx2)) {
                      ConstantInt *consIDX1 = dynamic_cast<ConstantInt*>(idx1);
                      ConstantInt *consIDX2 = dynamic_cast<ConstantInt*>(idx2);
                      // Get the sign extended value (for zero extended use ZExt)
                      int64_t intIDX1 = consIDX1->getSExtValue();
                      int64_t intIDX2 = consIDX2->getSExtValue();
                      if(intIDX1 == intIDX2) {
                        idxValid = true;
                      }
                    }
                    // Only if the UB and IDX of this pair is the same as the
                    // one we're examining in the intersect set do we mark it as
                    // valid (not removed). We'll keep track of all the
                    // duplicate checks to remove later in intersectMap
                    if(ubValid && idxValid) {
                      valid = true;
                      intersectMap[intersectPair].insert(currentPair);
                    }
                  }
                  if(!valid) {
                    // If the check wasn't in the current successor then it is
                    // removed from the intersection 
                    intersect.erase(intersectPair);
                  }
                }
              }
            }
            // A non-empty intersection set means that we are going to hoist so
            // mark the change flag
            if(!intersect.empty()) {
              change = true;
              // Iterate over all the intersections we found
              set<inspair>::iterator it;
              for(it=intersect.begin(); it != intersect.end(); ++it) {
                inspair current = *it;
                //errs() << *current.first << "\n";
                //errs() << *current.second << "\n";
                set<inspair>::iterator mapIt;
                // Remove the duplicates from the successor BBs
                if(intersectMap.find(current) != intersectMap.end()) {
                  for(mapIt=intersectMap[current].begin(); mapIt != intersectMap[current].end(); ++mapIt) {
                    inspair toDel = *mapIt;
                    toDel.first->eraseFromParent();
                    toDel.second->eraseFromParent();
                    numChecksHoisted++;
                  }
                }
                //errs() << *BB << "\n";
                // And hoist the original one we found into the current BB
                current.first->moveBefore(BB->getTerminator());
                current.second->moveBefore(BB->getTerminator());
                //errs() << *BB << "\n";
                numChecksHoisted++;
              }
            }
          }
        }

        // Step 3: Move appropriate checks out of the loop
        // (i) Check uses only definitions from outside the loop body
        set<inspair> propagated;
        set<BasicBlock*>::iterator it;
        for(it = loopDomBlocks.begin(); it != loopDomBlocks.end(); ++it) {
          BasicBlock *BB = *it;
          //errs() << *BB << "\n";
          for(BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I) {
            // Check to see if this instruction is an upper bounds check
            if(!I->getName().str().compare(0, 12, "_arrayref ub")) {
              bool valid = true;
              Instruction *Iub = &(*I);
              Instruction *Iidx = &(*(++I));
              // Pull out the actual values of the upper bound and index
              Instruction *ub = dynamic_cast<Instruction*>(Iub->getOperand(0));
              Instruction *idx = dynamic_cast<Instruction*>(Iidx->getOperand(0));
              // The move is only valid if any instructions UB or IDX reference
              // are not contained inside this loop. We check that the Loop L
              // doesn't contain the BasicBlock the referenced instruction
              // lives in.
              if(ub != NULL) {
                if(L->contains(ub->getParent())) {
                  valid = false;
                }
              }
              if(idx != NULL) {
                if(L->contains(idx->getParent())) {
                  valid = false;
                }
              }
              if(valid) {
                propagated.insert(make_pair(Iub, Iidx));
              }
            }
          }
        }
        // (ii) and (iii) involve checks of type lb<=i and i<=ub which is
        // irrelevant with the single check scheme used by our insertion method
        // (iv) is a subset of our Bounding pass and not included

        BasicBlock *preheader = L->getLoopPreheader();
        // Verify that there is a valid preheader and that it is well-formed
        // (has a terminator instruction)
        if(preheader == NULL || preheader->getTerminator() == NULL) {
          errs() << "No preheader detected. Nonstandard loop?\n";
        }
        else {
          // Move all checks in propagated out of the loop
          set<inspair>::iterator propIt;
          for(propIt = propagated.begin(); propIt != propagated.end(); ++propIt) {
            inspair I = *propIt;
            // Move the checks to the end of the preheader
            I.first->moveBefore(preheader->getTerminator());
            I.second->moveBefore(preheader->getTerminator());
            numChecksPropagated++;
          }
        }
      }
      // Return a boolean indicating if this function actually did anything
      return (oldHoisted != numChecksHoisted || oldPropagated != numChecksPropagated);
    }

    set<Instruction*> globalElimination(Function *F,
                                        map< BasicBlock*,set<inspair> > IN,
                                        map< BasicBlock*,set<inspair> > OUT,
                                        map< BasicBlock*,set<inspair> > GEN,
                                        map< BasicBlock*,set<inspair> > KILL)
    {
      // Used to track whether the iterative method has stabilized
      bool change = true;
      while(change) {
        change = false;
        // Iterate over all BasicBlocks in the function 
        for(Function::iterator FI = F->begin(), E_F = F->end(); FI != E_F; ++FI) {
          // Get the current BB
          BasicBlock* BB = &(*FI);
          // This set will represent the intersection of all predecessors OUT
          // sets
          set<inspair> intersect;
          // Iterate over all predecessors of the current BB
          for(pred_iterator PI = pred_begin(BB), E = pred_end(BB); PI != E; ++PI) {
            BasicBlock* pred = *PI; 
            // If we are on the first block just make the intersection
            // everything in the OUT set
            if(PI == pred_begin(BB)) {
              intersect.insert(OUT[pred].begin(), OUT[pred].end());
            }
            // Otherwise remove anything that isn't present in the OUT set of
            // the predecessor
            else {
              set<inspair> fail;
              set<inspair>::iterator it;
              // Go through each element currently in the intersect set
              for(it = intersect.begin(); it != intersect.end(); ++it) {
                // If this element of the intersect set does not appear in the
                // OUT set of the current predecessor we'll remove it from the
                // intersect set
                if(OUT[pred].count(*it) == 0) {
                  fail.insert(*it);
                }
              }
              // Go through and remove the appropriate elements from the
              // intersect set
              for(it = fail.begin(); it != fail.end(); ++it) {
                int test = intersect.erase(*it);
              }
            }
          }
          // Detected change in IN set so iterate again
          if(IN[BB] != intersect) {
            change = true;
          }
          IN[BB] = intersect;
          // The OUT set is just the union of the IN and GEN sets since the KILL
          // set is empty due to SSA
          set<inspair> newout;
          newout.insert(IN[BB].begin(), IN[BB].end());
          newout.insert(GEN[BB].begin(), GEN[BB].end());
          // Detected change in OUT set so iterate again
          if(OUT[BB] != newout) {
            change = true;
          }
          OUT[BB] = newout;
        }
      }

      set<Instruction*> insToDel;
      // If equivalent check is present in IN for BB we can delete this check 
      // Iterate over all BasicBlocks in the function 
      for(Function::iterator FI = F->begin(), FI_E = F->end(); FI != FI_E; ++FI) {
        BasicBlock *BB = &(*FI);
        // This will print out the IN sets for each BB in the function
        /*errs() << "\n __NEW BB__ \n";
        set<inspair>::iterator it;
        for(it=IN[BB].begin(); it != IN[BB].end(); ++it) {
          errs() << *((*it).first) << "\n";
          errs() << *((*it).second) << "\n";
        }*/
        for(BasicBlock::iterator I = FI->begin(), E = FI->end(); I != E; ++I) {
          // Check to see if this instruction is an upper bounds check
          if(!I->getName().str().compare(0, 12, "_arrayref ub")) {
            Instruction *Iub = &(*I);
            Instruction *Iidx = &(*(++I));
            // Pull out the actual values of the upper bound and index
            Value *ub = Iub->getOperand(0);
            Value *idx = Iidx->getOperand(0);
            // Check these values against all bounds checks present in IN
            set<inspair>::iterator it;
            for(it=IN[BB].begin(); it != IN[BB].end(); ++it) {
              // Grab the instructions and the actual values for this check from
              // the IN set
              Instruction *Iub2 = (*it).first;
              Instruction *Iidx2 = (*it).second;
              Value *ub2 = Iub2->getOperand(0);
              Value *idx2 = Iidx2->getOperand(0);
              bool dupeUB = false;
              bool dupeIDX = false;
              if(!isa<Instruction>(*ub) && !isa<Instruction>(*ub2)) {
                ConstantInt *consUB = dynamic_cast<ConstantInt*>(ub);
                ConstantInt *consUB2 = dynamic_cast<ConstantInt*>(ub2);
                // Get the sign extended value (for zero extended use ZExt)
                int64_t intUB = consUB->getSExtValue();
                int64_t intUB2 = consUB2->getSExtValue();
                // Check that the incoming upper bound (UB2) is at least as
                // strict as the current UB
                if(intUB >= intUB2) {
                  dupeUB = true;
                }                
              }
              else {
                if(ub == ub2) {
                  dupeUB = true;
                }
              }

              if(!isa<Instruction>(*idx) && !isa<Instruction>(*idx2)) {
                ConstantInt *consIDX = dynamic_cast<ConstantInt*>(idx);
                ConstantInt *consIDX2 = dynamic_cast<ConstantInt*>(idx2);
                // Get the sign extended value (for zero extended use ZExt)
                int64_t intIDX = consIDX->getSExtValue();
                int64_t intIDX2 = consIDX2->getSExtValue();
                // Check that the incoming upper bound (IDX2) is at least as
                // strict as the current IDX
                if(intIDX <= intIDX2 && intIDX >= 0) {
                  dupeIDX = true;
                }                
              }
              else {
                if(idx == idx2) {
                  dupeIDX = true;
                }
              }
              // If the current check is identical to one in the IN set remove it
              if(dupeUB && dupeIDX) {
                insToDel.insert(Iub);
                insToDel.insert(Iidx);
                //errs() << "GLOBAL Removed" << *Iub << "\n";
                //errs() << "GLOBAL Removed" << *Iidx << "\n";
              }
            }
          }
        }
      }
      return insToDel;
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
              int64_t ub = consUB->getSExtValue();
              
              // Pull out the upper bound LLVM representation
              ConstantInt *consUB2 = dynamic_cast<ConstantInt*>(ub2);
              // Get the sign extended value (for zero extended use ZExt)
              int64_t ub2 = consUB2->getSExtValue();

              // Duplicate if the upperbound of the examined instruction is at
              // least as stringent as the upperbound of an earlier instruction
              if (ub >= ub2) {
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
              int64_t idx = consIDX->getSExtValue();
              
              // Pull out the upper bound LLVM representation
              ConstantInt *consIDX2 = dynamic_cast<ConstantInt*>(idx2);
              // Get the sign extended value (for zero extended use ZExt)
              int64_t idx2 = consIDX2->getSExtValue();

              if (idx <= idx2 && idx >= 0) {
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
            checkedIns.insert(make_pair(ub, idx));
          }
          // If it is a dupe add both checks to the list of Instructions to
          // delete
          else {
            insToDel.insert(&(*(--I)));
            //errs() << "LOCAL  Removed" << *I << "\n";
            insToDel.insert(&(*(++I)));
            //errs() << "LOCAL  Removed" << *I << "\n";
          }
        }
      }
      return insToDel;
    }
      
    //Add required analyses here
    void getAnalysisUsage(AnalysisUsage &AU) const
    {
      AU.addRequired<LoopInfo>();
      AU.addRequired<DominatorTree>();
    }
  };

  char OptimizeChecks::ID = 0;
  RegisterPass<OptimizeChecks> X("OptimizeChecks", "Redundant check removal");
}
