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
