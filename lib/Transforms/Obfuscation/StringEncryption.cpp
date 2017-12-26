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
    set<GlobalVariable *> cstrings;
    set<GlobalVariable *> objcstrings;
    // Collect GVs
    for (auto g = M.global_begin(); g != M.global_end(); g++) {
      GlobalVariable *GV = &(*g);
      // We only handle NonMetadata&&NonObjC&&LocalInitialized&&CDS
      if (GV->hasInitializer() && GV->isConstant() &&
          GV->getSection() != StringRef("llvm.metadata") &&
          GV->getSection().find(StringRef("__objc")) == string::npos &&
          GV->getName().find("OBJC") == string::npos) {

        if (GV->hasInitializer() &&
            GV->getType()->getElementType() ==
                M.getTypeByName("struct.__NSConstantString_tag")) {
          objcstrings.insert(GV);
          continue;
        }
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
      HandleString(GV);
    }
    // TODO:Do post-run clean up
    return false;
  } // End runOnModule
  bool HandleString(GlobalVariable *GV) {
    ConstantDataSequential *CDS =
        dyn_cast<ConstantDataSequential>(GV->getInitializer());

    vector<uint8_t> stringvals;
    vector<uint8_t> keys;
    for (unsigned i = 0; i < CDS->getNumElements(); i++) {
      uint8_t key = cryptoutils->get_uint8_t(); // Random number in 0~255. which
                                                // covers all possible value of
                                                // a uint8_t
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
    GV->replaceAllUsesWith(EncryptedGV);
    GV->eraseFromParent();
    // TODO: Visit Instructions refering here and and decode functions
    for (User *U : EncryptedGV->users()) {
      /*if (Instruction *Inst = dyn_cast<Instruction>(U)) {
        errs() << "F is used in instruction:\n";
        errs() << *Inst << "\n";
      }*/
      U->dump();
    }
    return true;
  }
};
Pass *createStringEncryptionPass() { return new StringEncryption(); }
} // namespace llvm

char StringEncryption::ID = 0;
