#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Obfuscation/Obfuscation.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
using namespace llvm;
using namespace std;
namespace llvm {
struct IndirectBranch : public FunctionPass {
  static char ID;
  IndirectBranch() : FunctionPass(ID) {}
  StringRef getPassName() const override { return StringRef("IndirectBranch"); }
  bool runOnFunction(Function &Func) override {
    if (Func.isDeclaration()) {
      return false;
    }
    vector<BranchInst *> BIs;
    for (inst_iterator I = inst_begin(Func); I != inst_end(Func); I++) {
      Instruction *Inst = &(*I);
      if (BranchInst *BI = dyn_cast<BranchInst>(Inst)) {
        BIs.push_back(BI);
      }
    } // Finish collecting branching conditions
    for (BranchInst *BI : BIs) {
      IRBuilder<> IRB(BI);
      vector<BasicBlock *> BBs;
      // We use the condition's evaluation result to generate the GEP
      // instruction  False evaluates to 0 while true evaluates to 1.  So here we
      // insert the false block first
      if (BI->isConditional()) {
        BBs.push_back(BI->getSuccessor(1));
      }
      BBs.push_back(BI->getSuccessor(0));
      ArrayType *AT = ArrayType::get(
          Type::getInt8PtrTy(Func.getParent()->getContext()), BBs.size());
      vector<Constant *> BlockAddresses;
      for (unsigned i = 0; i < BBs.size(); i++) {
        BlockAddresses.push_back(BlockAddress::get(BBs[i]));
      }
      Constant *BlockAddressArray =
          ConstantArray::get(AT, ArrayRef<Constant *>(BlockAddresses));
      AllocaInst *allocated = IRB.CreateAlloca(AT);
      IRB.CreateStore(BlockAddressArray, allocated);
      Value *GEP = NULL;
      Value *zero =
          ConstantInt::get(Type::getInt32Ty(Func.getParent()->getContext()), 0);
      if (BI->isConditional()) {
        Value *condition = BI->getCondition();
        Value *sextcondition = IRB.CreateZExt(condition,Type::getInt32Ty(Func.getParent()->getContext()));
        GEP = IRB.CreateGEP(allocated, {zero,sextcondition});
      } else {
        GEP = IRB.CreateGEP(allocated, {zero, zero});
      }
      LoadInst *LI = IRB.CreateLoad(GEP, "IndirectBranchingTargetAddress");
      IndirectBrInst *indirBr = IndirectBrInst::Create(LI, BBs.size());
      for (BasicBlock *BB : BBs) {
        indirBr->addDestination(BB);
      }
      ReplaceInstWithInst(BI, indirBr);
    }
    return true;
  }
};
} // namespace llvm
FunctionPass *llvm::createIndirectBranchPass() { return new IndirectBranch(); }
char IndirectBranch::ID = 0;
INITIALIZE_PASS(IndirectBranch, "indibran", "IndirectBranching", true, true)
