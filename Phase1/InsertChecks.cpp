//LLVM Bounds Checking Insertion Pass
//CS 6241 Project Phase 1
//Brian Ouellette
//Chad Kersey
//Gaurav Gupta

//Run using:
//opt -load ../../Release/lib/Phase1.so -InsertChecks -time-passes < *.bc > /dev/null

#define DEBUG_TYPE "InsertChecks"

#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#include <stdint.h>

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/InstIterator.h"

#include "llvm/ADT/Statistic.h"

using namespace llvm;
using namespace std;

namespace
{
        STATISTIC(numStaticArrays, "Total number of static-sized arrays allocated.");

	class InsertChecks : public FunctionPass
	{
        public:
		static char ID;
		InsertChecks() : FunctionPass(&ID) {}

		virtual bool runOnFunction(Function &F)
		{
			errs() << "InsertChecks\n";

			// Start by finding all of the static array allocations.
                        // This is incomplete. Globals aren't alloca'd.
                        // It's also unnecessary since it seems that the pointer's type itself contains the array
                        // bounds.
                        numStaticArrays = 0;
			for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
                                AllocaInst *A = dynamic_cast<AllocaInst *>(&(*I));
                                if (A != NULL) {
                                  // A->isArrayAllocation() is always false, but the type of the pointer A returns
                                  // does include the size, which can be used to distinguish arrays.
                                  const Type *T = A->getType()->getTypeAtIndex(unsigned(0));
                                  const ArrayType *U = dynamic_cast<const ArrayType*>(T);
                                  if (U != NULL) {
                                    errs() << *A << '\n' << *U << '\n';
                                    numStaticArrays++;
                                  }
                                }
			}

			/* Create exit condition BB
			 * Check for getelementptr (array accesses) instructions
			 * Either get array size from inbounds keyword or from original array def (alloca)
			 * Add check that exits to new BB
			 * Cleanup CFG by splitting original BB?
			 */ 
			
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
