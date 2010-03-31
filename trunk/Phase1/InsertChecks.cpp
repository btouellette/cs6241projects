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
        STATISTIC(numArrayAccesses, "Total number of getElementPtr instructions.");

	class InsertChecks : public FunctionPass
	{
        public:
		static char ID;
		InsertChecks() : FunctionPass(&ID) {}

		virtual bool runOnFunction(Function &F)
		{
			errs() << "InsertChecks\n";

                        // Start by finding all of the getArrayIndex instances.
                        numArrayAccesses = 0;
			for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
                                GetElementPtrInst *P = dynamic_cast<GetElementPtrInst *>(&(*I));
                                if (P == NULL) continue;
                                const Type *T = P->getPointerOperand()->getType();
                                const PointerType *PT = dynamic_cast<const PointerType*>(T);
                                if (PT == NULL) continue;
                                const ArrayType *U = dynamic_cast<const ArrayType*>(PT->getTypeAtIndex(0u));
                                if (U == NULL) continue;

                                errs() << *P << '\n' << *U << '\n';
                                numArrayAccesses++;
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
