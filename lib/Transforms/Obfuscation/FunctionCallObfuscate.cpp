/*
 *  LLVM CallSite Obfuscation Pass
 *  It works by scanning all CallSites that refers to a function outside of
 *  current translation unit then replace then with dlopen/dlsym calls
    Copyright (C) 2017 Zhang(https://github.com/Naville/)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.
    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "json.hpp"
#include "llvm/ADT/Triple.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Obfuscation/Obfuscation.h"
#include <algorithm>
#include <cstdlib>
#include <dlfcn.h>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
using namespace llvm;
using namespace std;
using json = nlohmann::json;
static cl::opt<string>
    SymbolConfigPath("fcoconfig",
                     cl::desc("FunctionCallObfuscate Configuration Path"),
                     cl::value_desc("filename"), cl::init("+-x/"));
namespace llvm {
struct FunctionCallObfuscate : public FunctionPass {
  static char ID;
  json Configuration;
  bool flag;
  FunctionCallObfuscate() : FunctionPass(ID) { this->flag = true; }
  FunctionCallObfuscate(bool flag) : FunctionPass(ID) { this->flag = flag; }
  StringRef getPassName() const override {
    return StringRef("FunctionCallObfuscate");
  }
  virtual bool doInitialization(Module &M) override {
    // Basic Defs
    if (SymbolConfigPath == "+-x/") {
      SmallString<32> Path;
      if (sys::path::home_directory(Path)) { // Stolen from LineEditor.cpp
        sys::path::append(Path, "Hikari", "SymbolConfig.json");
        SymbolConfigPath = Path.str();
      }
    }
    ifstream infile(SymbolConfigPath);
    if (infile.good()) {
      errs() << "Loading Symbol Configuration From:" << SymbolConfigPath
             << "\n";
      infile >> this->Configuration;
    } else {
      errs() << "Failed To Loading Symbol Configuration From:"
             << SymbolConfigPath << "\n";
    }
    Triple tri(M.getTargetTriple());
    if (tri.getVendor() != Triple::VendorType::Apple) {
      return false;
    }
    Type *Int64Ty = Type::getInt64Ty(M.getContext());
    Type *Int32Ty = Type::getInt32Ty(M.getContext());
    Type *Int8PtrTy = Type::getInt8PtrTy(M.getContext());
    Type *Int8Ty = Type::getInt8Ty(M.getContext());
    // Generic ObjC Runtime Declarations
    FunctionType *IMPType =
        FunctionType::get(Int8PtrTy, {Int8PtrTy, Int8PtrTy}, true);
    PointerType *IMPPointerType = PointerType::get(IMPType, 0);
    vector<Type *> classReplaceMethodTypeArgs;
    classReplaceMethodTypeArgs.push_back(Int8PtrTy);
    classReplaceMethodTypeArgs.push_back(Int8PtrTy);
    classReplaceMethodTypeArgs.push_back(IMPPointerType);
    classReplaceMethodTypeArgs.push_back(Int8PtrTy);
    FunctionType *class_replaceMethod_type =
        FunctionType::get(IMPPointerType, classReplaceMethodTypeArgs, false);
    M.getOrInsertFunction("class_replaceMethod", class_replaceMethod_type);
    FunctionType *sel_registerName_type =
        FunctionType::get(Int8PtrTy, {Int8PtrTy}, false);
    M.getOrInsertFunction("sel_registerName", sel_registerName_type);
    FunctionType *objc_getClass_type =
        FunctionType::get(Int8PtrTy, {Int8PtrTy}, false);
    M.getOrInsertFunction("objc_getClass", objc_getClass_type);
    M.getOrInsertFunction("objc_getMetaClass", objc_getClass_type);
    StructType *objc_property_attribute_t_type = reinterpret_cast<StructType *>(
        M.getTypeByName("struct.objc_property_attribute_t"));
    if (objc_property_attribute_t_type == NULL) {
      vector<Type *> types;
      types.push_back(Int8PtrTy);
      types.push_back(Int8PtrTy);
      objc_property_attribute_t_type = StructType::create(
          ArrayRef<Type *>(types), "struct.objc_property_attribute_t");
      M.getOrInsertGlobal("struct.objc_property_attribute_t",
                          objc_property_attribute_t_type);
    }
    vector<Type *> allocaClsTypeVector;
    vector<Type *> addIvarTypeVector;
    vector<Type *> addPropTypeVector;
    allocaClsTypeVector.push_back(Int8PtrTy);
    allocaClsTypeVector.push_back(Int8PtrTy);
    addIvarTypeVector.push_back(Int8PtrTy);
    addIvarTypeVector.push_back(Int8PtrTy);
    addPropTypeVector.push_back(Int8PtrTy);
    addPropTypeVector.push_back(Int8PtrTy);
    addPropTypeVector.push_back(objc_property_attribute_t_type->getPointerTo());
    if (tri.isArch64Bit()) {
      // We are 64Bit Device
      allocaClsTypeVector.push_back(Int64Ty);
      addIvarTypeVector.push_back(Int64Ty);
      addPropTypeVector.push_back(Int64Ty);
    } else {
      // Not 64Bit.However we are still on apple platform.So We are
      // ARMV7/ARMV7S/i386
      // PowerPC is ignored, feel free to open a PR if you want to
      allocaClsTypeVector.push_back(Int32Ty);
      addIvarTypeVector.push_back(Int32Ty);
      addPropTypeVector.push_back(Int32Ty);
    }
    addIvarTypeVector.push_back(Int8Ty);
    addIvarTypeVector.push_back(Int8PtrTy);
    // Types Collected. Now Inject Functions
    FunctionType *addIvarType =
        FunctionType::get(Int8Ty, ArrayRef<Type *>(addIvarTypeVector), false);
    M.getOrInsertFunction("class_addIvar", addIvarType);
    FunctionType *addPropType =
        FunctionType::get(Int8Ty, ArrayRef<Type *>(addPropTypeVector), false);
    M.getOrInsertFunction("class_addProperty", addPropType);
    FunctionType *class_getName_Type =
        FunctionType::get(Int8PtrTy, {Int8PtrTy}, false);
    M.getOrInsertFunction("class_getName", class_getName_Type);
    FunctionType *objc_getMetaClass_Type =
        FunctionType::get(Int8PtrTy, {Int8PtrTy}, false);
    M.getOrInsertFunction("objc_getMetaClass", objc_getMetaClass_Type);
    return true;
  }
  void HandleObjC(Module &M) {
    // Iterate all CLASSREF uses and replace with objc_getClass() call
    // Strings are encrypted in other passes
    for (auto G = M.global_begin(); G != M.global_end(); G++) {
      GlobalVariable &GV = *G;
      if (GV.getName().str().find("OBJC_CLASSLIST_REFERENCES") == 0) {
        if (GV.hasInitializer()) {
          string className = GV.getInitializer()->getName();
          className.replace(className.find("OBJC_CLASS_$_"),
                            strlen("OBJC_CLASS_$_"), "");
          for (auto U = GV.user_begin(); U != GV.user_end(); U++) {
            if (Instruction *I = dyn_cast<Instruction>(*U)) {
              IRBuilder<> builder(I);
              Function *objc_getClass_Func =
                  cast<Function>(M.getFunction("objc_getClass"));
              Value *newClassName =
                  builder.CreateGlobalStringPtr(StringRef(className));
              CallInst *CI =
                  builder.CreateCall(objc_getClass_Func, {newClassName});
              // We need to bitcast it back to avoid IRVerifier
              Value *BCI = builder.CreateBitCast(CI, I->getType());
              I->replaceAllUsesWith(BCI);
              I->eraseFromParent();
            }
          }
          GV.removeDeadConstantUsers();
          if (GV.getNumUses() == 0) {
            GV.dropAllReferences();
            GV.eraseFromParent();
          }
        }
      }
      // Selector Convert
      else if (GV.getName().str().find("OBJC_SELECTOR_REFERENCES") == 0) {
        if (GV.hasInitializer()) {
          ConstantExpr *CE = dyn_cast<ConstantExpr>(GV.getInitializer());
          Constant *C = CE->getOperand(0);
          GlobalVariable *SELNameGV = dyn_cast<GlobalVariable>(C);
          ConstantDataArray *CDA =
              dyn_cast<ConstantDataArray>(SELNameGV->getInitializer());
          StringRef SELName = CDA->getAsString(); // This is REAL Selector Name
          for (auto U = GV.user_begin(); U != GV.user_end(); U++) {
            if (Instruction *I = dyn_cast<Instruction>(*U)) {
              IRBuilder<> builder(I);
              Function *sel_registerName_Func =
                  cast<Function>(M.getFunction("sel_registerName"));
              Value *newGlobalSELName = builder.CreateGlobalStringPtr(SELName);
              CallInst *CI =
                  builder.CreateCall(sel_registerName_Func, {newGlobalSELName});
              // We need to bitcast it back to avoid IRVerifier
              Value *BCI = builder.CreateBitCast(CI, I->getType());
              I->replaceAllUsesWith(BCI);
              I->eraseFromParent();
            }
          }
          GV.removeDeadConstantUsers();
          if (GV.getNumUses() == 0) {
            GV.dropAllReferences();
            GV.eraseFromParent();
          }
        }
      }
    }
  }
  virtual bool runOnFunction(Function &F) override {
    // Construct Function Prototypes
    if (toObfuscate(flag, &F, "fco") == false) {
      return false;
    }
    errs() << "Running FunctionCallObfuscate On " << F.getName()
           << "\n";
    Module *M = F.getParent();
    HandleObjC(*M);
    Type *Int32Ty = Type::getInt32Ty(M->getContext());
    Type *Int8PtrTy = Type::getInt8PtrTy(M->getContext());
    // ObjC Runtime Declarations
    FunctionType *dlopen_type = FunctionType::get(
        Int8PtrTy, {Int8PtrTy, Int32Ty},
        false); // int has a length of 32 on both 32/64bit platform
    FunctionType *dlsym_type =
        FunctionType::get(Int8PtrTy, {Int8PtrTy, Int8PtrTy}, false);
    Function *dlopen_decl =
        cast<Function>(M->getOrInsertFunction("dlopen", dlopen_type));
    Function *dlsym_decl =
        cast<Function>(M->getOrInsertFunction("dlsym", dlsym_type));
    // Begin Iteration
    for (BasicBlock &BB : F) {
      for (auto I = BB.getFirstInsertionPt(), end = BB.end(); I != end; ++I) {
        Instruction &Inst = *I;
        if (isa<CallInst>(&Inst) || isa<InvokeInst>(&Inst)) {
          CallSite CS(&Inst);
          Function *calledFunction = CS.getCalledFunction();
          if (calledFunction == NULL) {
            /*
              Note:
              For Indirect Calls:
                CalledFunction is NULL and calledValue is usually a bitcasted
              function pointer. We'll need to strip out the hiccups and obtain
              the called Function* from there
            */
            calledFunction =
                dyn_cast<Function>(CS.getCalledValue()->stripPointerCasts());
          }
          // Simple Extracting Failed
          // Use our own implementation
          if (calledFunction == NULL) {
            errs() << "Failed To Extract Function From Indirect Call\n";
            CS.getCalledValue()->print(errs());
            errs() << "\n";
            continue;
          }
          // It's only safe to restrict our modification to external symbols
          // Otherwise stripped binary will crash
          if (!calledFunction->empty() ||
              calledFunction->getName().equals("dlsym") ||
              calledFunction->getName().equals("dlopen") ||
              calledFunction->isIntrinsic()) {
            continue;
          }
          // errs()<<"Searching For:"<<calledFunction->getName()<<" In
          // Configuration\n";
          if (this->Configuration.find(calledFunction->getName().str()) !=
              this->Configuration.end()) {
            string sname = this->Configuration[calledFunction->getName().str()]
                               .get<string>();
            StringRef calledFunctionName = StringRef(sname);
            BasicBlock *EntryBlock = CS->getParent();
            IRBuilder<> IRB(EntryBlock, EntryBlock->getFirstInsertionPt());
            vector<Value *> dlopenargs;
            dlopenargs.push_back(Constant::getNullValue(Int8PtrTy));
            dlopenargs.push_back(
                ConstantInt::get(Int32Ty, RTLD_NOW | RTLD_GLOBAL));
            Value *Handle =
                IRB.CreateCall(dlopen_decl, ArrayRef<Value *>(dlopenargs));
            // Create dlsym call
            vector<Value *> args;
            args.push_back(Handle);
            args.push_back(IRB.CreateGlobalStringPtr(calledFunctionName));
            Value *fp = IRB.CreateCall(dlsym_decl, ArrayRef<Value *>(args));
            Value *bitCastedFunction =
                IRB.CreateBitCast(fp, CS.getCalledValue()->getType());
            CS.setCalledFunction(bitCastedFunction);
          }
        }
      }
    }
    return true;
  }
};
FunctionPass *createFunctionCallObfuscatePass() {
  return new FunctionCallObfuscate();
}
FunctionPass *createFunctionCallObfuscatePass(bool flag) {
  return new FunctionCallObfuscate(flag);
}
} // namespace llvm
char FunctionCallObfuscate::ID = 0;
INITIALIZE_PASS(FunctionCallObfuscate, "fcoobf",
                "Enable Function CallSite Obfuscation.", true, true)
