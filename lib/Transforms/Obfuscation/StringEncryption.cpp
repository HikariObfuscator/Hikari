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
#include <map>
#include <set>
#include <string>
/*
  Unlike Armariris which inject decrytion code at llvm.global_ctors.
  We try to find the containing Function of Users referencing our string GV.
  Then we search for terminators.
  We insert decryption code at begining or the function and encrypt it back at
  terminators

  For Users where we cant find a Function, we then inject decryption codes at
  ctors
*/
/*
Status: Currently we only handle C-Style strings passed in directly.
We don't support global strings or ObjC strings.
But adding corresponding support should be as easy as some kind of type check


*/
using namespace llvm;
using namespace std;
namespace llvm {
struct StringEncryption : public ModulePass {
  static char ID;
  map<GlobalVariable * /*Value*/, Constant * /*Key*/>
      keymap; // Map GV to keys for encryption
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
    EncryptGVs(M);
    return true;
  } // End runOnModule
  void HandleFunction(Function *Func) {
    if (Func->isDeclaration()) {
      return;
    }
    srand(time(NULL));
    set<GlobalVariable *> Globals;
    // boot-strappish code stolen from
    // https://stackoverflow.com/questions/25761390/how-to-find-which-global-variables-are-used-in-a-function-by-using-llvm-api
    // I'm sure def-use chain would be a lot more better
    // But for now let's save ourselves some trouble
    set<Instruction *> Terminators;
    for (BasicBlock &BB : *Func) {
      for (Instruction &I : BB) {
        if (TerminatorInst *TI = dyn_cast<TerminatorInst>(&I)) {
          Terminators.insert(TI);
        }
        for (Value *Op : I.operands()) {
          if (GlobalVariable *G = dyn_cast<GlobalVariable>(Op)) {
            Globals.insert(G);
          } else if (Constant *C = dyn_cast<Constant>(Op)) {
            Constant *stripped = C->stripPointerCasts();
            if (GlobalVariable *GV = dyn_cast<GlobalVariable>(stripped)) {
              Globals.insert(GV);
              continue;
            }
          }
        }
      }
    }
    // This removes any GV that is not applicable for encryption
    for (GlobalVariable *GV : Globals) {
      // TODO: Perform Actual Work
      // Note that we only perform instruction adding here.
      // Encrypting GVs are done in finalization to avoid encrypting GVs more
      // than once
      if (GV->hasInitializer() &&
          GV->getSection() != StringRef("llvm.metadata") &&
          GV->getSection().find(StringRef("__objc")) == string::npos &&
          GV->getName().find("OBJC") == string::npos) {
        if (ConstantDataSequential *CDS =
                dyn_cast<ConstantDataSequential>(GV->getInitializer())) {
          GV->setConstant(false);
          Type *memberType = CDS->getElementType();
          // Ignore non-IntegerType
          if (!isa<IntegerType>(memberType)) {
            continue;
          }
          IntegerType *intType = cast<IntegerType>(memberType);
          if (keymap.find(GV) == keymap.end()) {
            // No Existing Key Found.
            // Perform injection
            if (intType == Type::getInt8Ty(GV->getParent()->getContext())) {
              vector<uint8_t> keys;
              for (unsigned i = 0; i < CDS->getNumElements(); i++) {
                keys.push_back(cryptoutils->get_uint8_t());
              }
              Constant *KeyConst = ConstantDataVector::get(
                  GV->getParent()->getContext(), ArrayRef<uint8_t>(keys));
              keymap[GV] = KeyConst;
            } else if (intType ==
                       Type::getInt16Ty(GV->getParent()->getContext())) {
              vector<uint16_t> keys;
              for (unsigned i = 0; i < CDS->getNumElements(); i++) {
                keys.push_back(cryptoutils->get_uint16_t());
              }
              Constant *KeyConst = ConstantDataVector::get(
                  GV->getParent()->getContext(), ArrayRef<uint16_t>(keys));
              keymap[GV] = KeyConst;
            } else if (intType ==
                       Type::getInt32Ty(GV->getParent()->getContext())) {
              vector<uint32_t> keys;
              for (unsigned i = 0; i < CDS->getNumElements(); i++) {
                keys.push_back(cryptoutils->get_uint32_t());
              }
              Constant *KeyConst = ConstantDataVector::get(
                  GV->getParent()->getContext(), ArrayRef<uint32_t>(keys));
              keymap[GV] = KeyConst;
            } else if (intType ==
                       Type::getInt64Ty(GV->getParent()->getContext())) {
              vector<uint64_t> keys;
              for (unsigned i = 0; i < CDS->getNumElements(); i++) {
                keys.push_back(cryptoutils->get_uint64_t());
              }
              Constant *KeyConst = ConstantDataVector::get(
                  GV->getParent()->getContext(), ArrayRef<uint64_t>(keys));
              keymap[GV] = KeyConst;
            } else {
              errs() << "Unsupported CDS Type\n";
              abort();
            }
          }
          // ADD XOR DecodeInstructions
          IRBuilder<> IRB(Func->getEntryBlock().getFirstNonPHIOrDbgOrLifetime ());
          Value *zero = ConstantInt::get(
              Type::getInt32Ty(GV->getParent()->getContext()), 0);//Beginning Index
          Value *zeroes[] = {zero,zero};
          Value *GEP = IRB.CreateInBoundsGEP(GV, zeroes);
          //BinaryOperations don't take CDAs,only CDVs
          //FIXME: Figure out if CDA and CDV has same mem layout
          Value* BCI=IRB.CreateBitCast(GEP,keymap[GV]->getType()->getPointerTo());
          LoadInst *LI = IRB.CreateLoad(BCI);//ArrayType
          Value *XOR = IRB.CreateXor(LI, keymap[GV]);
          IRB.CreateStore(XOR, BCI);
          for (Instruction *I : Terminators) {
            IRBuilder<> IRB(I);
            Value *zero = ConstantInt::get(
                Type::getInt32Ty(GV->getParent()->getContext()), 0);
            Value *zeroes[] = {zero, zero};
            Value *GEP = IRB.CreateInBoundsGEP(GV, zeroes);
            Value* BCI=IRB.CreateBitCast(GEP,keymap[GV]->getType()->getPointerTo());
            LoadInst *LI = IRB.CreateLoad(BCI);//ArrayType
            Value *XOR = IRB.CreateXor(LI, keymap[GV]);
            IRB.CreateStore(XOR,BCI);
          }
        }
      }
    }

  } // End of HandleFunction
  void EncryptGVs(Module &M){
    // We've done Instruction Insertation
    // Perform GV Encrytion
    for (map<GlobalVariable *, Constant *>::iterator it = keymap.begin();
         it != keymap.end(); ++it) {
      GlobalVariable *GV = it->first;
      assert(GV->hasInitializer() && "Encrypted GV doesn't have initializer");
      ConstantDataSequential *Key = cast<ConstantDataSequential>(it->second);
      ConstantDataSequential *GVInitializer =
          cast<ConstantDataSequential>(GV->getInitializer());
      assert(Key->getNumElements() == GVInitializer->getNumElements() &&
             "Key and String size mismatch!");
      assert(Key->getElementType() == GVInitializer->getElementType() &&
                    "Key and String type mismatch!");
      Type *memberType = Key->getElementType();
      IntegerType *intType = cast<IntegerType>(memberType);
      //FIXME: ConstantDataArray returning ConstantAggregateZero

      if (intType == Type::getInt8Ty(M.getContext())) {
        vector<uint8_t> Encrypted;
        for (unsigned i = 0; i < Key->getNumElements(); i++) {
          uint64_t K = Key->getElementAsInteger(i);
          uint64_t S = GVInitializer->getElementAsInteger(i);
          Encrypted.push_back(K^S);
        }
        Constant* newInit=ConstantDataArray::get(M.getContext(),ArrayRef<uint8_t>(Encrypted));
        GV->setInitializer(newInit);
      }
      else if (intType == Type::getInt16Ty(M.getContext())) {
        vector<uint16_t> Encrypted;
        for (unsigned i = 0; i < Key->getNumElements(); i++) {
          uint64_t K = Key->getElementAsInteger(i);
          uint64_t S = GVInitializer->getElementAsInteger(i);
          Encrypted.push_back(K ^ S);
        }
        Constant* newInit=ConstantDataArray::get(M.getContext(),ArrayRef<uint16_t>(Encrypted));
        GV->setInitializer(newInit);
      } else if (intType == Type::getInt32Ty(M.getContext())) {
        vector<uint32_t> Encrypted;
        for (unsigned i = 0; i < Key->getNumElements(); i++) {
          uint64_t K = Key->getElementAsInteger(i);
          uint64_t S = GVInitializer->getElementAsInteger(i);
          Encrypted.push_back(K ^ S);
        }
        Constant* newInit=ConstantDataArray::get(M.getContext(),ArrayRef<uint32_t>(Encrypted));
        GV->setInitializer(newInit);
      } else if (intType == Type::getInt64Ty(M.getContext())) {
        vector<uint64_t> Encrypted;
        for (unsigned i = 0; i < Key->getNumElements(); i++) {
          uint64_t K = Key->getElementAsInteger(i);
          uint64_t S = GVInitializer->getElementAsInteger(i);
          Encrypted.push_back(K ^ S);
        }
        Constant* newInit=ConstantDataArray::get(M.getContext(),ArrayRef<uint64_t>(Encrypted));
        GV->setInitializer(newInit);

      } else {
        errs() << "Unsupported CDS Type\n"<<*intType<<"\n";
        abort();
      }
      errs()<<"Rewritten GlobalVariable:"<<*GVInitializer<<" To:"<<*(GV->getInitializer())<<"\n";
    }
  }
};
Pass *createStringEncryptionPass() { return new StringEncryption(); }
} // namespace llvm

char StringEncryption::ID = 0;
static RegisterPass<StringEncryption> X("strenc", "StringEncryption");
