//LLVM Bounds Checking Insertion Pass
//CS 6241 Project Phase 1
//Brian Ouellette
//Chad Kersey
//Gaurav Gupta

//Run using:
//opt -load ../../Release/lib/Phase1.so -InsertChecks -time-passes < *.bc > /dev/null

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/InstIterator.h"

using namespace llvm;

namespace
{
	struct InsertChecks : public FunctionPass
	{
		static char ID;
		InsertChecks() : FunctionPass(&ID) {}

		virtual bool runOnFunction(Function &F)
		{
			errs() << "InsertChecks\n";

			//Iterates over all instructions in the function
			for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I)
				errs() << *I << "\n";
			
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
