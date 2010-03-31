//LLVM Bounds Checking Optimization Pass
//CS 6241 Project Phase 1
//Brian Ouellette
//Chad Kersey
//Gaurav Gupta

//Run using:
//opt -load ../../Release/lib/Phase1.so -InsertChecks -OptimizeChecks -time-passes < *.bc > /dev/null

#define DEBUG_TYPE "Optimizechecks"

#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#include <stdint.h>

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace
{
	struct OptimizeChecks : public FunctionPass
	{
		static char ID;
		OptimizeChecks() : FunctionPass(&ID) {}

		virtual bool runOnFunction(Function &F)
		{
			errs() << "OptimizeChecks\n";
			//Possibly modified function so return true
			return true;
		}
		
		//Add required analyses here
		void getAnalysisUsage(AnalysisUsage &AU) const
		{
			
		}
	};

	char OptimizeChecks::ID = 0;
	RegisterPass<OptimizeChecks> X("OptimizeChecks", "Redundant bounds checking removal");
}
