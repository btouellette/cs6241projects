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

    virtual bool runOnFunction(Function &F)
    {
      errs() << "InsertChecks\n";

      const IntegerType *Int64Ty = Type::getInt64Ty(F.getContext());
      const Type *VoidTy = Type::getVoidTy(F.getContext());

      Module *M = F.getParent();
 
      // Find all getElementPtr instances and add instrumentation.
	  // Iterate over Instructions in Function
      for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {

		// Ensure instruction is of GEP type
        GetElementPtrInst *P = dynamic_cast<GetElementPtrInst *>(&(*I));
        if (P == NULL) continue;

        const Type *T = P->getPointerOperand()->getType();
        const PointerType *PT = dynamic_cast<const PointerType*>(T);
        if (PT == NULL) continue;

		// Retrieve the array being dereferenced 
        const ArrayType *U = 
          dynamic_cast<const ArrayType*>(PT->getTypeAtIndex(0u));
        if (U == NULL) continue;
        
        // Get array size.
        const uint64_t n = U->getNumElements();
        errs() << *P << '\n' << *U << '\n' << n << " elements.\n\n";
        numArrayAccesses++;

        // Add call to bounds-checking function before the getElementPtr. This 
        // should get inlined away.
        vector<const Type*> ArgTypes(2);
        ArgTypes[0] = ArgTypes[1] = Int64Ty;
        FunctionType *ChkType = FunctionType::get(VoidTy, ArgTypes, false);
		// Insert or retrieve the checking function into the program Module
        Constant *Chk = M->getOrInsertFunction("__checkArrayBounds", ChkType);

		// Inject the correct indexes into the checking function
        Value* args[] = {ConstantInt::get(Int64Ty, n, true), P->getOperand(2)};
        CallInst *CI = CallInst::Create(Chk, &args[0], &args[2], "", &(*I));

        numChecksAdded++;
    }

      //Possibly modified function so return true
      return true;
    }
		
    //Add required analyses here
    void getAnalysisUsage(AnalysisUsage &AU) const
    {
      AU.setPreservesCFG();
    }
  };

  char InsertChecks::ID = 0;
  RegisterPass<InsertChecks> X("InsertChecks", "Bounds checking insertion");
}
