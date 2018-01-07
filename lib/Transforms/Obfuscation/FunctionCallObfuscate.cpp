/*
 *  LLVM CallSite Obfuscation Pass
 *  https://github.com/Naville
 *  GPL V3 Licensed
 *  This is designed to be a LTO pass and should be executed at LTO stage.
 *  It works by scanning all CallSites that refers to a function outside of
 * current translation unit then replace then with dlopen/dlsym calls
 */

#include "llvm/Transforms/Obfuscation/Obfuscation.h"
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
  FunctionCallObfuscate() : FunctionPass(ID) {}
  StringRef getPassName()const override{return StringRef("FunctionCallObfuscate");}
  bool doInitialization(Module &M) override {
    if (SymbolConfigPath == "+-x/") {
      SmallString<32> Path;
      if (sys::path::home_directory(Path)) { // Stolen from LineEditor.cpp
        sys::path::append(Path, "Hikari", "SymbolConfig.json");
        SymbolConfigPath = Path.str();
      }
    }
    errs() << "Loading Symbol Configuration From:" << SymbolConfigPath << "\n";
    ifstream infile(SymbolConfigPath);
    infile >> this->Configuration;
    return false;
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
              FunctionType *objc_getClass_type = FunctionType::get(
                  I->getType(), {Type::getInt8PtrTy(M.getContext())}, false);
              Function *objc_getClass_Func = cast<Function>(
                  M.getOrInsertFunction("objc_getClass", objc_getClass_type));
              Value *newClassName =
                  builder.CreateGlobalStringPtr(StringRef(className));
              CallInst *CI =
                  builder.CreateCall(objc_getClass_Func, {newClassName});
              I->replaceAllUsesWith(CI);
              I->eraseFromParent();
            }
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
              FunctionType *sel_registerName_type = FunctionType::get(
                  I->getType(), {Type::getInt8PtrTy(M.getContext())}, false);
              Function *sel_registerName_Func =
                  cast<Function>(M.getOrInsertFunction("sel_registerName",
                                                       sel_registerName_type));
              Value *newGlobalSELName = builder.CreateGlobalStringPtr(SELName);
              CallInst *CI =
                  builder.CreateCall(sel_registerName_Func, {newGlobalSELName});
              I->replaceAllUsesWith(CI);
              I->eraseFromParent();
            }
          }
        }
      }
    }
  }
  virtual bool runOnFunction(Function &F) override {
    // Construct Function Prototypes
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
            CS.getCalledValue()->print(errs());
            errs() << " Not Applicable For FCO\n";
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
            // errs() << "Created dlopen() Instruction:";
            // Handle->print(errs());
            // errs() << "\n";
            // Hack. See issues #1
            // errs() << "Fixed SymbolName:" << calledFunction->getName()<< "
            // To:" << calledFunctionName << "\n";

            // Create dlsym call
            vector<Value *> args;
            args.push_back(Handle);
            args.push_back(IRB.CreateGlobalStringPtr(calledFunctionName));
            Value *fp = IRB.CreateCall(dlsym_decl, ArrayRef<Value *>(args));
            Value *bitCastedFunction =
                IRB.CreateBitCast(fp, CS.getCalledValue()->getType());
            CS.setCalledFunction(bitCastedFunction);
            // errs() << "Created dlsym() Instruction:";
            // fp->print(errs());
            //  errs() << " For Function:";
            // calledFunction->print(errs());
            // errs() << "\n";
          }
        }
      }
    }
    return true;
  }
};
Pass *createFunctionCallObfuscatePass() { return new FunctionCallObfuscate(); }
} // namespace llvm
char FunctionCallObfuscate::ID = 0;
INITIALIZE_PASS_BEGIN(FunctionCallObfuscate, "fcoobf", "Enable Function CallSite Obfuscation.",true,true)
INITIALIZE_PASS_DEPENDENCY(AntiClassDump)
INITIALIZE_PASS_END(FunctionCallObfuscate, "fcoobf", "Enable Function CallSite Obfuscation.",true,true)
