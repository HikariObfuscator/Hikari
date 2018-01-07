/*
 *  LLVM StringEncryption Pass
 *  https://github.com/Naville
 *  GPL V3 Licensed
 */
#include "llvm/Transforms/Obfuscation/StringEncryption.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Obfuscation/CryptoUtils.h"
#include "llvm/Transforms/Obfuscation/Obfuscation.h"
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <string>
using namespace llvm;
using namespace std;
namespace llvm {
struct StringEncryption : public ModulePass {
  static char ID;
  StringEncryption() : ModulePass(ID) {}
  StringRef getPassName() const override {
    return StringRef("StringEncryption");
  }
  bool runOnModule(Module &M) override {
    // in runOnModule. We simple iterate function list and dispatch functions
    // to handlers
    for (Module::iterator iter = M.begin(); iter != M.end(); iter++) {
      Function &F = *iter;
      HandleFunction(&F);
    }
    return true;
  } // End runOnModule
  void FixFunctionConstantExpr(Function* Func){

    if (Func->isDeclaration()) {
      return;
    }
    IRBuilder<> IRB(Func->getEntryBlock().getFirstNonPHIOrDbgOrLifetime());
    set<GlobalVariable *> Globals;
    set<User *> Users;
    //Replace ConstantExpr with equal instructions
    //Otherwise replacing on Constant will crash the compiler
    for (BasicBlock &BB : *Func) {
      for (Instruction &I : BB) {
        for (Value *Op : I.operands()) {
          if (ConstantExpr *C = dyn_cast<ConstantExpr>(Op)) {
            Instruction* Inst=IRB.Insert(C->getAsInstruction());
            I.replaceUsesOfWith(C,Inst);
          }
        }
      }
    }
  }//End FixFunctionConstantExpr
  void HandleFunction(Function *Func) {
    if (Func->isDeclaration()) {
      return;
    }
    FixFunctionConstantExpr(Func);
    IRBuilder<> IRB(Func->getEntryBlock().getFirstNonPHIOrDbgOrLifetime());
    set<GlobalVariable *> Globals;
    set<User *> Users;
    for (BasicBlock &BB : *Func) {
      for (Instruction &I : BB) {
        for (Value *Op : I.operands()) {
          if (GlobalVariable *G = dyn_cast<GlobalVariable>(Op)) {
            Users.insert(&I);
            Globals.insert(G);
          }
        }
      }
    }
    set<GlobalVariable *> rawStrings;
    set<GlobalVariable *> objCStrings;
    map<GlobalVariable * /*Original C String*/, Value *>
        encmap; // Map Original CString to dyn-generated value for use in ObjC
                // String Transform

    for (GlobalVariable *GV : Globals) {
      if (GV->hasInitializer() &&
          GV->getSection() != StringRef("llvm.metadata") &&
          GV->getSection().find(StringRef("__objc")) == string::npos &&
          GV->getName().find("OBJC") == string::npos) {
        if (GV->getInitializer()->getType() ==
            Func->getParent()->getTypeByName("struct.__NSConstantString_tag")) {
          rawStrings.insert(cast<GlobalVariable>(cast<ConstantStruct>(GV->getInitializer())->getOperand(2)->stripPointerCasts()));
          objCStrings.insert(GV);

        } else if (isa<ConstantDataSequential>(GV->getInitializer())) {
          rawStrings.insert(GV);
        }
      }
    }
    for (GlobalVariable *GV : rawStrings) {
      if(GV->getInitializer()->isZeroValue()||GV->getInitializer()->isNullValue()){
        continue;
      }
      ConstantDataSequential* CDS=cast<ConstantDataSequential>(GV->getInitializer());
      Type *memberType = CDS->getElementType();
      // Ignore non-IntegerType
      if (!isa<IntegerType>(memberType)) {
        continue;
      }
      IntegerType *intType = cast<IntegerType>(memberType);
      Constant *KeyConst = NULL;
      Constant *EncryptedConst = NULL;
      if (intType == Type::getInt8Ty(GV->getParent()->getContext())) {
        vector<uint8_t> keys;
        vector<uint8_t> encry;
        for (unsigned i = 0; i < CDS->getNumElements(); i++) {
          uint8_t K = cryptoutils->get_uint8_t();
          uint64_t V = CDS->getElementAsInteger(i);
          keys.push_back(K);
          encry.push_back(K ^ V);
        }
        KeyConst = ConstantDataVector::get(GV->getParent()->getContext(),
                                           ArrayRef<uint8_t>(keys));
        EncryptedConst = ConstantDataVector::get(GV->getParent()->getContext(),
                                                 ArrayRef<uint8_t>(encry));
      } else if (intType == Type::getInt16Ty(GV->getParent()->getContext())) {
        vector<uint16_t> keys;
        vector<uint16_t> encry;
        for (unsigned i = 0; i < CDS->getNumElements(); i++) {
          uint16_t K = cryptoutils->get_uint16_t();
          uint64_t V = CDS->getElementAsInteger(i);
          keys.push_back(K);
          encry.push_back(K ^ V);
        }
        KeyConst = ConstantDataVector::get(GV->getParent()->getContext(),
                                           ArrayRef<uint16_t>(keys));
        EncryptedConst = ConstantDataVector::get(GV->getParent()->getContext(),
                                                 ArrayRef<uint16_t>(encry));
      } else if (intType == Type::getInt32Ty(GV->getParent()->getContext())) {
        vector<uint32_t> keys;
        vector<uint32_t> encry;
        for (unsigned i = 0; i < CDS->getNumElements(); i++) {
          uint32_t K = cryptoutils->get_uint32_t();
          uint64_t V = CDS->getElementAsInteger(i);
          keys.push_back(K);
          encry.push_back(K ^ V);
        }
        KeyConst = ConstantDataVector::get(GV->getParent()->getContext(),
                                           ArrayRef<uint32_t>(keys));
        EncryptedConst = ConstantDataVector::get(GV->getParent()->getContext(),
                                                 ArrayRef<uint32_t>(encry));
      } else if (intType == Type::getInt64Ty(GV->getParent()->getContext())) {
        vector<uint64_t> keys;
        vector<uint64_t> encry;
        for (unsigned i = 0; i < CDS->getNumElements(); i++) {
          uint64_t K = cryptoutils->get_uint64_t();
          uint64_t V = CDS->getElementAsInteger(i);
          keys.push_back(K);
          encry.push_back(K ^ V);
        }
        KeyConst = ConstantDataVector::get(GV->getParent()->getContext(),
                                           ArrayRef<uint64_t>(keys));
        EncryptedConst = ConstantDataVector::get(GV->getParent()->getContext(),
                                                 ArrayRef<uint64_t>(encry));
      } else {
        errs() << "Unsupported CDS Type\n";
        abort();
      }
      // Setup XORed value

      // ADD XOR DecodeInstructions
      // Allocate A New Value On Stack And Perform XORing
      // Do a Store+Load+XOR to avoid any potential optimizations
      // Note we cant force bitcast from CDA to CDV here
      // The backend will get fucked up if we do so
      AllocaInst *allocated =
          IRB.CreateAlloca(KeyConst->getType());
      //Value* BCI=IRB.CreateBitCast(allocated,EncryptedConst->getType()->getPointerTo());
      IRB.CreateStore(EncryptedConst,allocated);
      LoadInst *LI = IRB.CreateLoad(allocated);
      Value *XORInst = IRB.CreateXor(LI, KeyConst);
      IRB.CreateStore(XORInst,allocated);
      encmap[GV] = IRB.CreateBitCast(allocated,CDS->getType()->getPointerTo());
      for (User *U : Users) {
          U->replaceUsesOfWith(GV,encmap[GV]);
      }
    }
    for (GlobalVariable *GV : objCStrings) {
      ConstantStruct* CS=cast<ConstantStruct>(GV->getInitializer());
      GlobalVariable* DecryptedString=cast<GlobalVariable>(CS->getOperand(2)->stripPointerCasts());
      if(DecryptedString->getInitializer()->isZeroValue()||DecryptedString->getInitializer()->isNullValue()){
        continue;
      }
      AllocaInst *allocatedCFString = IRB.CreateAlloca(
          Func->getParent()->getTypeByName("struct.__NSConstantString_tag"));
      Value *zero =
          ConstantInt::get(Type::getInt32Ty(GV->getParent()->getContext()), 0);
      Value *one =
          ConstantInt::get(Type::getInt32Ty(GV->getParent()->getContext()), 1);
      Value *two =
          ConstantInt::get(Type::getInt32Ty(GV->getParent()->getContext()), 2);
      Value *three =
          ConstantInt::get(Type::getInt32Ty(GV->getParent()->getContext()), 3);
      IRB.CreateStore(CS->getOperand(0),IRB.CreateGEP(allocatedCFString,{zero,zero}));
      IRB.CreateStore(CS->getOperand(1),IRB.CreateGEP(allocatedCFString,{zero,one}));
      Value* stroffset=IRB.CreateGEP(allocatedCFString,{zero,two});
      Value* BCI=IRB.CreateBitCast(encmap[DecryptedString],CS->getOperand(2)->getType());
      IRB.CreateStore(BCI,stroffset);
      IRB.CreateStore(CS->getOperand(3),IRB.CreateGEP(allocatedCFString,{zero,three}));
      for (User *U : Users) {
          U->replaceUsesOfWith(GV,allocatedCFString);
      }
    }
    for(GlobalVariable *GV : objCStrings){
      GV->removeDeadConstantUsers ();
      //errs()<<*GV<<"  "<<GV->getNumUses ()<<"\n";
      if(GV->getNumUses()==0){
        GV->eraseFromParent();
      }
    }
    for(GlobalVariable *GV : rawStrings){
      GV->removeDeadConstantUsers ();
      //errs()<<*GV<<"  "<<GV->getNumUses ()<<"\n";
      if(GV->getNumUses()==0){
        GV->eraseFromParent();
      }
    }
    //FIXME: Replace uses
  } // End of HandleFunction
};
Pass *createStringEncryptionPass() { return new StringEncryption(); }
} // namespace llvm

char StringEncryption::ID = 0;
INITIALIZE_PASS_BEGIN(StringEncryption, "strcry", "Enable String Encryption",true,true)
INITIALIZE_PASS_DEPENDENCY(AntiClassDump)
INITIALIZE_PASS_DEPENDENCY(FunctionCallObfuscate)
INITIALIZE_PASS_END(StringEncryption, "strcry", "Enable String Encryption",true,true)
