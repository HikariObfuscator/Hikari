/*
 *  LLVM AntiDebugging Pass
 *  https://github.com/Naville
 *  GPL V3 Licensed
 */
#define IOS64ANTIDBG "MOV X0,26\nMOV X1,31\nMOV X2,0\nMOV X3,0\nMOV X16,0\nSVC 128"
#define IOS32ANTIDBG "MOV R0,31\nMOV R1,0\nMOV R2,0\nMOV R12,26\nSVC 128"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/Obfuscation/AntiDebugging.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/TargetSelect.h"
#include <string>
#include <iostream>
#include <cstdlib>
#include "llvm/IR/InlineAsm.h"
#include "llvm/ADT/Triple.h"
using namespace llvm;
using namespace std;
namespace llvm{
  struct AntiDebugging : public FunctionPass {
    static char ID;
    AntiDebugging() : FunctionPass(ID) {}
    bool runOnFunction(Function &F) override {
      BasicBlock *EntryBlock = &(F.getEntryBlock());
      IRBuilder<> IRB(EntryBlock, EntryBlock->getFirstInsertionPt());
      FunctionType *ADBFTy =FunctionType::get(Type::getVoidTy(F.getParent()->getContext()),ArrayRef<Type*>(), false);
      InlineAsm *IA =NULL;
      Triple tri(F.getParent()->getTargetTriple());
      Triple::OSType ost=tri.getOS();
      if(ost==Triple::OSType::IOS){
        errs()<<"Injecting Inline Assembly AntiDebugging For:"<<F.getParent()->getTargetTriple()<<"\n";
        StringRef InlineASMString;
        if(tri.isArch64Bit()){
          //We are 64bit iOS. i.e. AArch64
          InlineASMString=IOS64ANTIDBG;
        }
        else{
          InlineASMString=IOS32ANTIDBG;
        }
        IA=InlineAsm::get(ADBFTy,InlineASMString,"",true, false, InlineAsm::AD_Intel);
        IRB.CreateCall(IA, ArrayRef<Value*>());
      }
      else{
        //TODO: Support macOS's 32/64bit syscall
        errs()<<F.getParent()->getTargetTriple()<<" Unsupported AntiDebugging Target\n";
      }
      return true;
    }
  };
  Pass* createAntiDebuggingPass(){
    return new AntiDebugging();
  }
}

char AntiDebugging::ID = 0;
