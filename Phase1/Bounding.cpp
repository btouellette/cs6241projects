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
#include <list>

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
      list <Instruction *>toDelete;

      typedef Loop::block_iterator l_it;
      typedef BasicBlock::iterator b_it;
      typedef Value::use_iterator u_it;

      bool modified = false;

      if (L->getLoopPreheader() == NULL) return false;

      // Iterate over basic blocks in the loop. Looking for array references
      // using induction variables, which will be held in memory, because no
      // pointer analysis can have happened yet.
      for (l_it B = L->block_begin(); B != L->block_end(); B++) {
        for (b_it I = (*B)->begin(); I != (*B)->end(); I++) {
          if (I->getName().str().compare(0, 12, "_arrayref ub")) continue;
          
          Instruction *I0 = &(*I), *I1 = &(*(++I));

          Value *ub = I0->getOperand(0);
          Value *idx = I1->getOperand(0);
            
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
          Value *loop_lb = NULL, *loop_ub = NULL, *ub_ptr = NULL;
          ICmpInst *cmpInst = NULL;
          LoadInst *incLoad = NULL, *cmpLoad = NULL, *ub_load = NULL;
          ConstantInt *incVal = NULL;
          BranchInst *loopB = NULL;
          BinaryOperator *incInst = NULL;
          bool ubInclusive = false;

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
          bool abort = false;
          for (b_it I = (L->getHeader())->begin();
               I != (L->getHeader())->end();
               I++) {
            for (u_it J = P->use_begin(); J != P->use_end(); J++) {
              if (dyn_cast<Instruction>(I) == dyn_cast<Instruction>(J) &&
                  (cmpLoad = dyn_cast<LoadInst>(I)) &&
                  cmpLoad->hasNUses(1) &&
                  (cmpInst = dyn_cast<ICmpInst>(*(cmpLoad->use_begin()))) &&
                  cmpInst->hasNUses(1) &&
                  (loopB = dyn_cast<BranchInst>(*(cmpInst->use_begin()))) &&
                  loopB->getNumSuccessors() == 2) {
                Value *O0=cmpInst->getOperand(0), *O1=cmpInst->getOperand(1);
                bool trueExits = L->contains(loopB->getSuccessor(1));
                bool boundOnLeft = O1 == I;
                
                CmpInst::Predicate p = cmpInst->getPredicate();
                if (trueExits) {
                  switch(p) {
                    case CmpInst::ICMP_EQ:  p = CmpInst::ICMP_NE;  break;
                    case CmpInst::ICMP_NE:  p = CmpInst::ICMP_EQ;  break;
                    case CmpInst::ICMP_SGT: p = CmpInst::ICMP_SLE; break;
                    case CmpInst::ICMP_UGT: p = CmpInst::ICMP_ULE; break;
                    case CmpInst::ICMP_SLT: p = CmpInst::ICMP_SGE; break;
                    case CmpInst::ICMP_ULT: p = CmpInst::ICMP_UGE; break;
                    case CmpInst::ICMP_SGE: p = CmpInst::ICMP_SLT; break;
                    case CmpInst::ICMP_UGE: p = CmpInst::ICMP_ULT; break;
                    case CmpInst::ICMP_SLE: p = CmpInst::ICMP_SGT; break;
                    case CmpInst::ICMP_ULE: p = CmpInst::ICMP_UGT; break;
                    default: abort = true;
                  }
                }

                switch(cmpInst->getPredicate()) {

                case CmpInst::ICMP_NE:
                  ubInclusive = false;
                  if (trueExits) abort = true;
                  break;

                case CmpInst::ICMP_SGT:
                  ubInclusive = false;
                  if (!boundOnLeft) abort = true;
                  break;

                case CmpInst::ICMP_SLT:
                  ubInclusive = false;
                  if (boundOnLeft) abort = true;
                  break;

                case CmpInst::ICMP_ULT:
                  ubInclusive = false;
                  if (boundOnLeft) abort = true;
                  break;

                case CmpInst::ICMP_UGT:
                  ubInclusive = false;
                  if (!boundOnLeft) abort = true;
                  break;
 
                case CmpInst::ICMP_SLE:
                  ubInclusive = true;
                  if (boundOnLeft) abort = true;
                  break;

                case CmpInst::ICMP_SGE:
                  ubInclusive = true;
                  if (!boundOnLeft) abort = true;
                  break;

                case CmpInst::ICMP_ULE:
                  ubInclusive = true;
                  if (boundOnLeft) abort = true;
                  break;

                case CmpInst::ICMP_UGE:
                  ubInclusive = true;
                  break;

                default:
                  abort = true;
                }
                loop_ub = boundOnLeft?O0:O1;

                if (ub_load = dyn_cast<LoadInst>(loop_ub)) {
                  ub_ptr = ub_load->getPointerOperand();
                } else if (dyn_cast<ConstantInt>(loop_ub)) {
                } else {
                  abort = true;
                }
              }
            }
          }
          
          if (loop_ub == NULL || abort == true) {
            continue;
          }

          // Find the increment.
          for (l_it B = L->block_begin(); B != L->block_end() && !incVal; B++) {
            for (b_it I = (*B)->begin(); I != (*B)->end() && !incVal; I++) {
              if ((incStore = dyn_cast<StoreInst>(I)) && 
                  incStore->getPointerOperand() == P && incStore != lbStore &&
                  (incInst=dyn_cast<BinaryOperator>(incStore->getOperand(0))) &&
                  incInst->getOpcode() == Instruction::Add) {
                if (dyn_cast<LoadInst>(incInst->getOperand(0))) {
                  incLoad = dyn_cast<LoadInst>(incInst->getOperand(0));
                  incVal = dyn_cast<ConstantInt>(incInst->getOperand(1));
                } else if (dyn_cast<LoadInst>(incInst->getOperand(1))) {
                  incLoad = dyn_cast<LoadInst>(incInst->getOperand(1));
                  incVal = dyn_cast<ConstantInt>(incInst->getOperand(0));
                } else {
                  incLoad = NULL;
                }
              }
            }
          }

          if (incLoad == NULL || incVal == NULL) continue;
          
          // If there's a store to the loop induction variable other than the
          // increment or initialization, this optimization cannot happen.

          // If there's a store to the loop upper bound during the loop, this
          // optimization cannot happen.
          for (l_it B = L->block_begin(); B != L->block_end(); B++) {
            for (b_it I = (*B)->begin(); I != (*B)->end(); I++) {
              StoreInst *S;
              if (S = dyn_cast<StoreInst>(I)) {
                if (S->getPointerOperand() == P && S != incStore) {
                  abort = true;
                } else if (ub_ptr && S->getPointerOperand() == ub_ptr) {
                  abort = true;
                }
              }
              if (ub_ptr == &(*I)) abort = true;
            }
          }
          if (abort) continue;

          numChecksMoved++; modified = true;

          // Move the check to the loop preheader.

          // 1. Flag the original bounds check instructions for deletion.
          toDelete.insert(toDelete.end(), I0); 
          toDelete.insert(toDelete.end(), I1);

          // 2. Assemble the new bounds check instructions.
          if (ub_ptr) {
            loop_ub = new LoadInst(
              ub_ptr, "", L->getLoopPreheader()->getTerminator()
            );
          }

          if (!ubInclusive) {
            ub = BinaryOperator::CreateNSWAdd(
              ub, ConstantInt::get(
                ub->getType(), 1, 
                true
              ), "", L->getLoopPreheader()->getTerminator()
            );
          }

          Instruction::CastOps UBCast, IdxCast;

          unsigned loopubBits = loop_ub->getType()->getPrimitiveSizeInBits();
          unsigned arrayubBits = ub->getType()->getPrimitiveSizeInBits();
          
          if (loopubBits == 64) IdxCast = Instruction::BitCast;
          else if (loopubBits < 64) IdxCast = Instruction::ZExt;
          else IdxCast = Instruction::Trunc;

          if (arrayubBits == 64) UBCast = Instruction::BitCast;
          else if (arrayubBits < 64) UBCast = Instruction::ZExt;
          else UBCast = Instruction::Trunc;

          // 3. Place the new bounds check instructions in the loop preheader.
          CastInst::Create(UBCast,
                           ub,
                           Type::getInt64Ty(
                             L->getHeader()->getParent()->getContext()
                           ),
                           "_arrayref ub", 
                           L->getLoopPreheader()->getTerminator());
          CastInst::Create(IdxCast,
                           loop_ub,
                           Type::getInt64Ty(
                             L->getHeader()->getParent()->getContext()
                           ),
                           "_arrayref idx",
                           L->getLoopPreheader()->getTerminator());
        }
      }

      for (list<Instruction*>::iterator i = toDelete.begin(); 
           i != toDelete.end(); i++) {
        (*i)->eraseFromParent();
      }

      return modified;
    }

  };

  char BoundChecks::ID = 0;
  RegisterPass<BoundChecks> X("BoundChecks", "Hoist checks from bounded loops");
}
