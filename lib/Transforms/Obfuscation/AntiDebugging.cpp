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
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/Obfuscation/AntiDebugging.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IRReader/IRReader.h"
#include <string>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include "llvm/IR/InlineAsm.h"
#include "llvm/ADT/Triple.h"
using namespace llvm;
using namespace std;
static cl::opt<string>
    PreCompiledIRPath("adbextirpath",
                     cl::desc("External Path Pointing To Pre-compiled IR.See Wiki"),
                     cl::value_desc("filename"), cl::init("+-x/"));
namespace llvm{
  struct AntiDebugging : public ModulePass {
    static char ID;
    AntiDebugging() : ModulePass(ID) {}
    StringRef getPassName()const override{return StringRef("AntiDebugging");}
    virtual bool doInitialization(Module &M) override {
      if (PreCompiledIRPath == "+-x/") {
        SmallString<32> Path;
        if (sys::path::home_directory(Path)) { // Stolen from LineEditor.cpp
          sys::path::append(Path, "Hikari");
          Triple tri(M.getTargetTriple());
          sys::path::append(Path,"PrecompiledAntiDebugging-"+Triple::getArchTypeName(tri.getArch())+"-"+Triple::getOSTypeName(tri.getOS())+".bc");
          PreCompiledIRPath = Path.str();
        }
      }
      ifstream f(PreCompiledIRPath);
      if(f.good()){
        errs() << "Linking PreCompiled AntiDebugging IR From:" << PreCompiledIRPath << "\n";
        SMDiagnostic SMD;
        unique_ptr<Module> ADBM(parseIRFile(StringRef(PreCompiledIRPath),SMD,M.getContext()));
        SMD.print("Hikari",errs());
        Linker::linkModules(M,std::move(ADBM),Linker::Flags::OverrideFromSrc);
        //FIXME: Mess with GV in ADBCallBack
        return true;
      }
      else{
        errs() << "Failed To Link PreCompiled AntiDebugging IR From:" << PreCompiledIRPath << "\n";
        return false;
      }
    }
    bool runOnModule(Module &M) override{
      Function *ADBCallBack=M.getFunction("ADBCallBack");
      if(ADBCallBack){
        assert(!ADBCallBack->isDeclaration()&&"AntiDebuggingCallback is not concrete!");
        ADBCallBack->setVisibility(GlobalValue::VisibilityTypes::HiddenVisibility);
        ADBCallBack->setLinkage(GlobalValue::LinkageTypes::PrivateLinkage);
        ADBCallBack->removeFnAttr(Attribute::AttrKind::NoInline);
        ADBCallBack->removeFnAttr(Attribute::AttrKind::OptimizeNone);
        ADBCallBack->addFnAttr(Attribute::AttrKind::AlwaysInline);
      }
      Function *ADBInit=M.getFunction("InitADB");
      if(ADBInit){
        assert(!ADBInit->isDeclaration()&&"AntiDebuggingInitializer is not concrete!");
        ADBInit->setVisibility(GlobalValue::VisibilityTypes::HiddenVisibility);
        ADBInit->setLinkage(GlobalValue::LinkageTypes::PrivateLinkage);
        ADBInit->removeFnAttr(Attribute::AttrKind::NoInline);
        ADBInit->removeFnAttr(Attribute::AttrKind::OptimizeNone);
        ADBInit->addFnAttr(Attribute::AttrKind::AlwaysInline);
      }
      for (Module::iterator iter = M.begin(); iter != M.end(); iter++) {
        Function &F = *iter;
        if (!F.isDeclaration() && F.getName()!="ADBCallBack" && F.getName()!="InitADB") {
          runOnFunction(F);
        }
      }
      return true;
    }
    bool runOnFunction(Function &F){
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
        errs()<<F.getParent()->getTargetTriple()<<" Unsupported Inline Assembly AntiDebugging Target\n";
      }
      //Now operate on Linked AntiDBGCallbacks
      Function *ADBCallBack=F.getParent()->getFunction("ADBCallBack");
      Function *ADBInit=F.getParent()->getFunction("InitADB");
      if(ADBCallBack && ADBInit){
        IRB.CreateCall(ADBInit, ArrayRef<Value*>());
      }
      return true;
    }
  };
  ModulePass* createAntiDebuggingPass(){
    return new AntiDebugging();
  }
}

char AntiDebugging::ID = 0;
INITIALIZE_PASS(AntiDebugging, "adb", "Enable AntiDebugging.",true, true)
