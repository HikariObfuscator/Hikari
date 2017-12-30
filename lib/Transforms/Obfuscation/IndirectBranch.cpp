/*
 *  LLVM IndirectBranch Pass
 *  https://github.com/Naville
 *  GPL V3 Licensed
 */
#include "llvm/IR/Instructions.h"
#include "llvm/Transforms/Obfuscation/IndirectBranch.h"
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/IR/IRBuilder.h"
#include <string>
#include <iostream>
#include <cstdlib>
#include "llvm/ADT/Triple.h"
using namespace llvm;
using namespace std;
namespace llvm{
  struct IndirectBranch : public BasicBlockPass {
    static char ID;
    IndirectBranch() : BasicBlockPass(ID) {}
    StringRef getPassName()const override{return StringRef("IndirectBranch");}
    bool runOnBasicBlock(BasicBlock &BB) override {
      for (Instruction &I : BB){
        Instruction *Inst=&I;
        if(BranchInst* BI=dyn_cast<BranchInst>(Inst)){
          if(BI->isUnconditional()&&BI->getNumSuccessors()==1){
            //Conditional Branching Requires
            //Analyzing the condition and obtain
            //Runtime values corresponding target blocks
            //Let's skip those ATM
            BasicBlock* successor=BI->getSuccessor(0);
            if(!successor->hasName ()){
              successor->setName(Twine("IndirectBranchingTarget"));
            }
            IRBuilder<> IRB(BI);
            BlockAddress* BA=BlockAddress::get(successor);
            IndirectBrInst* IBI=IRB.CreateIndirectBr(BA);
            BI->replaceAllUsesWith(IBI);
            IBI->addDestination (successor);

            //BI->eraseFromParent(); WTF why this would crash
          }
        }
      }
      return false;
    }
  };
  Pass* createIndirectBranchPass(){
    return new IndirectBranch();
  }
  static RegisterPass<IndirectBranch> X("indibran", "Indirect Branching Obfuscation");
}

char IndirectBranch::ID = 0;
