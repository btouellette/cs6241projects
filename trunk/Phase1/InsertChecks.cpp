/*
 LLVM Bounds Checking Insertion Pass
 CS 6241 Project Phase 1
 Brian Ouellette
 Chad Kersey
 Gaurav Gupta

 Run using:
 opt -load ../../Release/lib/Phase1.so -InsertChecks -time-passes < *.bc > \
  /dev/null

 Issue: Prevent code from trying to run on structs which are also dereferenced
        via GEP
*/

#define DEBUG_TYPE "InsertChecks"

#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#include <stdint.h>

#include <vector>

#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/InstIterator.h"

#include "llvm/ADT/Statistic.h"

using namespace llvm;
using namespace std;

namespace
{
  STATISTIC(numArrayAccesses, "Number of getElementPtr instructions.");
  STATISTIC(numChecksAdded, "Number of bounds checks inserted.");

  class InsertChecks : public FunctionPass
  {
  public:
    static char ID;
    InsertChecks() : FunctionPass(&ID) {}

    virtual bool doInitialization(Module &M) 
    {
      numArrayAccesses = numChecksAdded = 0;
    }

    virtual bool runOnFunction(Function &F)
    {
      const IntegerType *Int64Ty = Type::getInt64Ty(F.getContext());
      const Type *VoidTy = Type::getVoidTy(F.getContext());

      Module *M = F.getParent();
 
      // Find all annotated lines and add call to bounds check.
      Value *prev_ub = 0;
      for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
        if (!I->getName().str().compare(0, 12, "_arrayref ub")) prev_ub = &(*I);
        if (!I->getName().str().compare(0, 13, "_arrayref idx")) {
          numArrayAccesses++;
          numChecksAdded++;
          
          // Add bounds-checking function call
          vector<const Type*> ArgTypes(2);
          ArgTypes[0] = ArgTypes[1] = Int64Ty;
          FunctionType *ChkType = FunctionType::get(VoidTy, ArgTypes, false);
          // Insert or retrieve the checking function into the program Module
          Constant *Chk = M->getOrInsertFunction("__checkArrayBounds", ChkType);

          // Inject the correct indexes into the checking function
          Value* args[] = {prev_ub, &(*I)};
          Value* const* ArgsBeg = args;
          Value* const* ArgsEnd = args + 2;
          CallInst *CI = CallInst::Create(Chk, ArgsBeg, ArgsEnd, "");
          CI->insertAfter(&(*I));
        }
      }
      //Possibly modified function so return true
      return true;
    }
		
    //Add required analyses here
    void getAnalysisUsage(AnalysisUsage &AU) const
    {

    }
  };

  char InsertChecks::ID = 0;
  RegisterPass<InsertChecks> X("InsertChecks", "Bounds checking insertion");
}
