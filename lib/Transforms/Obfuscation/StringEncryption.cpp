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
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
/*
  Unlike Armariris which inject decrytion code at llvm.global_ctors.
  We try to find the containing Function of Users referencing our string GV.
  Then we search for terminators.
  We insert decryption code at begining or the function and encrypt it back at terminators

  For Users where we cant find a Function, we then inject decryption codes at runtime
*/
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
    //FunctionType* decryptorType=FunctionType::get(Type::getVoidTy(M.getContext()),false);
    //Function* decryptor=Function::Create(decryptorType,GlobalValue::LinkageTypes::PrivateLinkage,"Decryptor",&M);
    set<GlobalVariable *> cstrings;
    set<GlobalVariable *> objcstrings;
    // Collect GVs
    for (auto g = M.global_begin(); g != M.global_end(); g++) {
      GlobalVariable *GV = &(*g);
      if (GV->hasInitializer()&&GV->getType()->getElementType() ==
              M.getTypeByName("struct.__NSConstantString_tag")) {
        objcstrings.insert(GV);
        continue;
      }
      // We only handle NonMetadata&&NonObjC&&LocalInitialized&&CDS
      if (GV->hasInitializer() && GV->isConstant() &&
          GV->getSection() != StringRef("llvm.metadata") &&
          GV->getSection().find(StringRef("__objc")) == string::npos &&
          GV->getName().find("OBJC") == string::npos) {
        // isString() asssumes the array has type i8, which should hold on all
        // major platforms  We don't care about some custom os written by 8yo
        // Bob that uses arbitrary ABI
        if (ConstantDataSequential *CDS =
                dyn_cast<ConstantDataSequential>(GV->getInitializer())) {
          if (CDS->isString()) {
            cstrings.insert(GV);
          }
        }
      }
    }
    // FIXME: Correctly Handle ObjC String Constants
    // Probably recreate at runtime and replace pointers?
    for (GlobalVariable *GV : objcstrings) {
      //@_unnamed_cfstring_ = private global %struct.__NSConstantString_tag {
      // i32* getelementptr inbounds ([0 x i32], [0 x i32]*
      //@__CFConstantStringClassReference, i32 0, i32 0),  i32 1992, i8*
      // getelementptr inbounds ([2 x i8], [2 x i8]* @.str, i32 0, i32 0), i64 1
      // },  section "__DATA,__cfstring", align 8
      ConstantStruct *CS = dyn_cast<ConstantStruct>(GV->getInitializer());
      Constant *CE = CS->getOperand(2); // This is GEP CE
      GlobalVariable *referencedGV =
          dyn_cast<GlobalVariable>(CE->getOperand(0));
      cstrings.erase(referencedGV);
    }
    for (GlobalVariable *GV : cstrings) {
      //errs()<<"Found C-StyleString\n";
      //GV->dump();
      HandleCString(GV);
    }
    for (GlobalVariable *GV : objcstrings) {
      //errs()<<"Found Objective-C-StyleString\n";
      //GV->dump();
      //HandleString(GV);
    }
    // TODO:Do post-run clean up
    return false;
  } // End runOnModule
  bool HandleCString(GlobalVariable *GV) {
    ConstantDataSequential *CDS =
        dyn_cast<ConstantDataSequential>(GV->getInitializer());
    assert(CDS && "GlobalVariable Passed in doesn't have ConstantDataSequential Initializer!");
    vector<uint8_t> stringvals;
    vector<uint8_t> keys;
    for (unsigned i = 0; i < CDS->getNumElements(); i++) {
      uint8_t key = cryptoutils->get_uint8_t();
      keys.push_back(key);
      uint8_t str = CDS->getElementAsInteger(i) ^ key;
      stringvals.push_back(str);
    }
    GlobalVariable *EncryptedGV = new GlobalVariable(
        *(GV->getParent()), CDS->getType(), false, GV->getLinkage(),
        ConstantDataArray::get(GV->getParent()->getContext(),
                               ArrayRef<uint8_t>(stringvals)),
        GV->getName() + "Encrypted", nullptr, GV->getThreadLocalMode(),
        GV->getType()->getAddressSpace());
    GlobalVariable *KeyGV = new GlobalVariable(
        *(GV->getParent()), CDS->getType(), false, GV->getLinkage(),
        ConstantDataArray::get(GV->getParent()->getContext(),
                               ArrayRef<uint8_t>(keys)),
        GV->getName() + "Keys", nullptr, GV->getThreadLocalMode(),
        GV->getType()->getAddressSpace());
    EncryptedGV->copyAttributesFrom(GV);
    addDecryption(GV,EncryptedGV,KeyGV);
    //Let's do this in addDecryption
    //GV->replaceAllUsesWith(EncryptedGV);
    //GV->eraseFromParent();
    return true;
  }//End of Handle CString

  /*
  We assume that the originalGV is only references from functions
  And all references of the GV is within one function
  */
  void addDecryption(GlobalVariable *origGV,GlobalVariable* enc,GlobalVariable* key){
    for (User *U : origGV->users()) {
      if (Instruction *Inst = dyn_cast<Instruction>(U)) {
        errs()<<"Found Instruction Usage of String GV\n";
        //Inst->dump();
      }
      else if(ConstantExpr* CE=dyn_cast<ConstantExpr>(U)){
        errs()<<"Found ConstantExpr Usage of String GV\n";
        //CE->dump();
      }
      else{
        errs()<<"Unknown Reference To String GV\n";
        //U->dump();
      }
    }
  }
};
Pass *createStringEncryptionPass() { return new StringEncryption(); }
} // namespace llvm

char StringEncryption::ID = 0;
static RegisterPass<StringEncryption> X("strenc", "StringEncryption");
