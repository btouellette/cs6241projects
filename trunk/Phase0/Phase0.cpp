//LLVM Statistics Pass
//CS 6241 Project Phase 0
//Brian Ouellette
//Run using:
//opt -load ../../Release/lib/Phase0.so -Phase0 -time-passes < *.bc > /dev/null

#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Function.h"
#include "llvm/BasicBlock.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"

#include <limits.h>
#include <set>

using namespace llvm;

namespace {

	struct Phase0 : public ModulePass {
		static char ID;
		Phase0() : ModulePass(&ID) {}
		
		virtual bool runOnModule(Module &M)
		{
			//Grab the CallGraph that represents this module
			CallGraph &CG = getAnalysis<CallGraph>();
		
			int num_func = 0;	
			int total_num_BB = 0;
			int total_CG_edges = 0;
			int total_num_loops = 0;
			int total_CFG_edges = 0;
			int total_loop_BB = 0;
			int total_dom_of_BB = 0;
			int total_dom_by_BB = 0;

			/*//We need a starting number to set the max and min number of BasicBlocks to so we'll just grab the first function and get the number of blocks there to use
			int max_BB = M.getFunctionList().begin()->getBasicBlockList().size();
			int min_BB = max_BB;*/
			//Just using integer max and min since first function may be a declaration or non-existent
			int max_BB = INT_MIN;
			int min_BB = INT_MAX;
			
			/*//Do the same for the max and min number of CallGraph edges
			int max_CG_edges = CG.begin()->second->size();
			int min_CG_edges = max_CG_edges;*/
			int max_CFG_edges = INT_MIN;
			int min_CFG_edges = INT_MAX;
			
			int max_loops = INT_MIN;
			int min_loops = INT_MAX;
			
			int max_loop_BB = INT_MIN;
			int min_loop_BB = INT_MAX;
			
			int max_dom_of_BB = INT_MIN;
			int min_dom_of_BB = INT_MAX;
			
			int max_dom_by_BB = INT_MIN;
			int min_dom_by_BB = INT_MAX;
			
			//*********(1)*********
			//TODO: Don't include return edges? How to differentiate?
			for(CallGraph::iterator CG_it = CG.begin(), CG_end = CG.end(); CG_it != CG_end; ++CG_it)
			{
				CallGraphNode* CGN = CG_it->second;
				int current_CG_edges = CGN->size(); 
				total_CG_edges += current_CG_edges;
			}
			
			//Iterate through all the Functions in the Module
			for (Module::iterator module_it = M.begin(), M_end = M.end(); module_it != M_end; ++module_it)
			{
				Function &F = *module_it;
				
				if(!F.isDeclaration())
				{
					//use getAnalysis to obtain information about the loops and dominance present
					LoopInfo &LI = getAnalysis<LoopInfo>(F);
					DominatorTree &DT = getAnalysis<DominatorTree>(F);
					//CallGraph &CG_func = getAnalysis<CallGraph>(F);
										
					//*********(2)*********
					//Avg, max, min number of basic blocks inside function
					int current_num_BB = F.getBasicBlockList().size();
				
					if(current_num_BB != 0)
					{
						num_func++;
						//Add the current functions number of basic blocks to the total and then set appropriate max and mins if they need to change
						total_num_BB += current_num_BB;
						if(current_num_BB > max_BB)
						{
							max_BB = current_num_BB;
						}
						if(current_num_BB < min_BB)
						{
							min_BB = current_num_BB;
						}
						
						int current_loop_BB  = 0;
						int current_CFG_edges = 0;
						//Iterate through all the BasicBlocks in the Function
						for(Function::iterator func_it = F.begin(), F_end = F.end(); func_it != F_end; ++func_it)
						{
							BasicBlock* B = func_it;
							current_loop_BB += LI.getLoopDepth(B);
							current_CFG_edges += B->getTerminator()->getNumSuccessors();
							
							//*********(5,7)*********
							//Avg, max, min number of doms of BBs inside function
							//Check whether the current BB against all other BBs in the function for dom relationships
							
							int current_dom_of_BB = 0;
							int current_dom_by_BB = 0;
							
							for(Function::iterator inner_it = F.begin(), it_end = F.end(); inner_it != it_end; ++inner_it)
							{
								BasicBlock* B_in = inner_it;
																
								//If B (current block) dominates B_in
								if(DT.dominates(B, B_in))
								{
									current_dom_by_BB++;
								}
								
								//If B (current block) is dominated by B_in
								if(DT.dominates(B_in, B))
								{
									current_dom_of_BB++;
								}
							}
							
							total_dom_of_BB += current_dom_of_BB;
							total_dom_by_BB += current_dom_by_BB;
							
							if(current_dom_of_BB > max_dom_of_BB)
							{
								max_dom_of_BB = current_dom_of_BB;
							}
							if(current_dom_of_BB < min_dom_of_BB)
							{
								min_dom_of_BB = current_dom_of_BB;
							}
							
							if(current_dom_by_BB > max_dom_by_BB)
							{
								max_dom_by_BB = current_dom_by_BB;
							}
							if(current_dom_by_BB < min_dom_by_BB)
							{
								min_dom_by_BB = current_dom_by_BB;
							}
						}
						
						//*********(3)*********
						//Avg, max, min number of CFG edges inside function
						total_CFG_edges += current_CFG_edges;
						if(current_CFG_edges > max_CFG_edges)
						{
							max_CFG_edges = current_CFG_edges;
						}
						if(current_CFG_edges < min_CFG_edges)
						{
							min_CFG_edges = current_CFG_edges;
						}						
						
						//*********(6)*********
						//Avg, max, min number of loop basic blocks inside function
						total_loop_BB += current_loop_BB;
						if(current_loop_BB > max_loop_BB)
						{
							max_loop_BB = current_loop_BB;
						}
						if(current_loop_BB < min_loop_BB)
						{
							min_loop_BB = current_loop_BB;
						}
					}
					
					//*********(4)*********
					//Avg, max, min number of single entry loops inside function
					int current_num_loops = 0;
					
					//Iterate over all the top level loops in the function
					for(LoopInfoBase<BasicBlock, Loop>::iterator LIB = LI.begin(), LIB_end = LI.end(); LIB != LIB_end; ++LIB)
					{
						Loop* L = *LIB;
						current_num_loops += countSubLoops(L);
					}
					total_num_loops += current_num_loops;
					if(current_num_loops > max_loops)
					{
						max_loops = current_num_loops;
					}
					if(current_num_loops < min_loops)
					{
						min_loops = current_num_loops;
					}
				}
			}
			
			errs() << "\n----(1)----\n";
			errs() << "Total Functions: " << num_func << "\n";
			errs() << "Total SCG Edges: " << total_CG_edges << "\n";
			
			errs() << "\n----(2)----\n";
			errs() << "Avg BB: " << format("%0.2f", (double)total_num_BB/num_func) << "\n";
			errs() << "Max BB: " << max_BB << "\n";
			errs() << "Min BB: " << min_BB << "\n";
			
			errs() << "\n----(3)----\n";
			errs() << "Avg CFG Edges: " << format("%0.2f", (double)total_CFG_edges/num_func) << "\n";
			errs() << "Max CFG Edges: " << max_CFG_edges << "\n";
			errs() << "Min CFG Edges: " << min_CFG_edges << "\n";
			
			errs() << "\n----(4)----\n";
			errs() << "Avg Single Entry Loops: " << format("%0.2f", (double)total_num_loops/num_func) << "\n";
			errs() << "Max Single Entry Loops: " << max_loops << "\n";
			errs() << "Min Single Entry Loops: " << min_loops << "\n";
			
			errs() << "\n----(5)----\n";
			errs() << "Avg Doms of BBs: " << format("%0.2f", (double)total_dom_of_BB/total_num_BB) << "\n";
			errs() << "Max Doms of BBs: " << max_dom_of_BB << "\n";
			errs() << "Min Doms of BBs: " << min_dom_of_BB << "\n";
			
			errs() << "\n----(6)----\n";
			errs() << "Avg Loop BBs: " << format("%0.2f", (double)total_loop_BB/num_func) << "\n";
			errs() << "Max Loop BBs: " << max_loop_BB << "\n";
			errs() << "Min Loop BBs: " << min_loop_BB << "\n";
			
			errs() << "\n----(7)----\n";
			errs() << "Avg Doms by BB: " << format("%0.2f", (double)total_dom_by_BB/total_num_BB) << "\n";
			errs() << "Max Doms by BB: " << max_dom_by_BB << "\n";
			errs() << "Min Doms by BB: " << min_dom_by_BB << "\n";
			
			unreachableCodeElimination(M);
			
			//We potentially modified the module in the unreachable code elimination so we return true
			return true;
		}
		
		//Since we don't know the loop nesting depth use recursion to count the number of loops contained inside Loop L
		//This will return 1 for a loop with no subloops (counting itself)
		int countSubLoops(Loop *L)
		{
			int loopCount = 1;
			if(!L->empty())
			{
				for(LoopInfoBase<BasicBlock, Loop>::iterator LI = L->begin(), LI_end = L->end(); LI != LI_end; ++LI)
				{
					Loop* L_in = *LI;
					loopCount += countSubLoops(L_in);
				}
			}			
			return loopCount;
		}
		
		//Implement unreachable code detection and elimination 
		void unreachableCodeElimination(Module &M)
		{
			//Iterate through all the Functions in the Module
			for(Module::iterator module_it = M.begin(), M_end = M.end(); module_it != M_end; ++module_it)
			{
				Function &F = *module_it;
				
				//Only do the optimization if Function is a definition, not a declaration
				if(!F.isDeclaration())
				{
					std::set<BasicBlock*> reachableBBs;
					std::set<BasicBlock*> flaggedBBs;
					
					//Entry BB is always reachable
					reachableBBs.insert(&(F.getEntryBlock()));
					
					do
					{
						//Clear out the set of new reachable BBs
						flaggedBBs.clear();
						
						//Iterate through the set of reachable BBs and add their successors to the flagged list if they aren't in the reachable set already
						for(std::set<BasicBlock*>::iterator set_it = reachableBBs.begin(), set_end = reachableBBs.end(); set_it != set_end; ++set_it)
						{
							BasicBlock* B = *set_it;
							int num_succ = B->getTerminator()->getNumSuccessors();
							for(int i = 0; i < num_succ; ++i)
							{
								BasicBlock* B_succ = B->getTerminator()->getSuccessor(i);
								if(reachableBBs.count(B_succ) == 0)
								{
									flaggedBBs.insert(B_succ);
								}
							}
						}
						
						//Iterate through all the recently flagged BBs and put them into the reachable set
						for(std::set<BasicBlock*>::iterator set_it = flaggedBBs.begin(), set_end = flaggedBBs.end(); set_it != set_end; ++set_it)
						{
							BasicBlock* B = *set_it;
							reachableBBs.insert(B);
						}
					} while(!flaggedBBs.empty());
					
					int total_removed_BBs = 0;
					
					//Iterate through all the BasicBlocks in the Function and remove them if they are not reachable
					for(Function::iterator func_it = F.begin(), F_end = F.end(); func_it != F_end; ++func_it)
					{
						BasicBlock* B = func_it;
						if(reachableBBs.count(B) == 0)
						{
							B->removeFromParent();
							total_removed_BBs++;
						}
					}
					
					//errs() << "UCE removed (" << total_removed_BBs << ") BasicBlocks" << "\n";
				}
			}
		}
	
		virtual void getAnalysisUsage(AnalysisUsage &AU) const
		{
			AU.setPreservesAll();
			AU.addRequired<CallGraph>();
			AU.addRequired<LoopInfo>();
			AU.addRequired<DominatorTree>();
		}
	};

	char Phase0::ID = 0;

	RegisterPass<Phase0> X("Phase0", "6241 Project Pass");
}
