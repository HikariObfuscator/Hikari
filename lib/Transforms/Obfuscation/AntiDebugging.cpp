/*
 *  LLVM AntiDebugging Pass
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
#include "llvm/ADT/Triple.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Obfuscation/Obfuscation.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
using namespace llvm;
using namespace std;
static cl::opt<string> PreCompiledIRPath(
    "adbextirpath",
    cl::desc(
        "External Path Pointing To Pre-compiled AntiDebugging IR.See Wiki"),
    cl::value_desc("filename"), cl::init(""));
namespace llvm {
struct AntiDebugging : public ModulePass {
  static char ID;
  bool flag;
  bool initialized;
  AntiDebugging() : ModulePass(ID) { this->flag = true;this->initialized=false;}
  AntiDebugging(bool flag) : ModulePass(ID) { this->flag = flag;this->initialized=false;}
  StringRef getPassName() const override { return StringRef("AntiDebugging"); }
  bool Initialize(Module &M){
    if (PreCompiledIRPath == "") {
      SmallString<32> Path;
      if (sys::path::home_directory(Path)) { // Stolen from LineEditor.cpp
        sys::path::append(Path, "Hikari");
        Triple tri(M.getTargetTriple());
        sys::path::append(Path, "PrecompiledAntiDebugging-" +
                                    Triple::getArchTypeName(tri.getArch()) +
                                    "-" + Triple::getOSTypeName(tri.getOS()) +
                                    ".bc");
        PreCompiledIRPath = Path.str();
      }
    }
    ifstream f(PreCompiledIRPath);
    if (f.good()) {
      errs() << "Linking PreCompiled AntiDebugging IR From:"
             << PreCompiledIRPath << "\n";
      SMDiagnostic SMD;
      unique_ptr<Module> ADBM(
          parseIRFile(StringRef(PreCompiledIRPath), SMD, M.getContext()));
      // SMD.print("Hikari", errs());
      Linker::linkModules(M, std::move(ADBM), Linker::Flags::LinkOnlyNeeded);
      // FIXME: Mess with GV in ADBCallBack
      Function *ADBCallBack = M.getFunction("ADBCallBack");
      if (ADBCallBack) {
        assert(!ADBCallBack->isDeclaration() &&
               "AntiDebuggingCallback is not concrete!");
        ADBCallBack->setVisibility(
            GlobalValue::VisibilityTypes::HiddenVisibility);
        ADBCallBack->setLinkage(GlobalValue::LinkageTypes::PrivateLinkage);
        ADBCallBack->removeFnAttr(Attribute::AttrKind::NoInline);
        ADBCallBack->removeFnAttr(Attribute::AttrKind::OptimizeNone);
        ADBCallBack->addFnAttr(Attribute::AttrKind::AlwaysInline);
      }
      Function *ADBInit = M.getFunction("InitADB");
      if (ADBInit) {
        assert(!ADBInit->isDeclaration() &&
               "AntiDebuggingInitializer is not concrete!");
        ADBInit->setVisibility(GlobalValue::VisibilityTypes::HiddenVisibility);
        ADBInit->setLinkage(GlobalValue::LinkageTypes::PrivateLinkage);
        ADBInit->removeFnAttr(Attribute::AttrKind::NoInline);
        ADBInit->removeFnAttr(Attribute::AttrKind::OptimizeNone);
        ADBInit->addFnAttr(Attribute::AttrKind::AlwaysInline);
      }
      return true;
    } else {
      errs() << "Failed To Link PreCompiled AntiDebugging IR From:"
             << PreCompiledIRPath << "\n";
      return false;
    }

  }
  bool runOnModule(Module &M) override {
    for (Module::iterator iter = M.begin(); iter != M.end(); iter++) {
      Function &F = *iter;
      if (toObfuscate(flag, &F, "adb") && F.getName() != "ADBCallBack" &&
          F.getName() != "InitADB") {
        runOnFunction(F);
      }
    }
    return true;
  }
  bool runOnFunction(Function &F) {
    errs() << "Running AntiDebugging On " << F.getName() << "\n";
    if(this->initialized==false){
      Initialize(*F.getParent());
      this->initialized=true;
    }
    BasicBlock *EntryBlock = &(F.getEntryBlock());
    IRBuilder<> IRB(EntryBlock, EntryBlock->getFirstInsertionPt());
    // Now operate on Linked AntiDBGCallbacks
    Function *ADBCallBack = F.getParent()->getFunction("ADBCallBack");
    Function *ADBInit = F.getParent()->getFunction("InitADB");
    if (ADBCallBack && ADBInit) {
      IRB.CreateCall(ADBInit, ArrayRef<Value *>());
    }
    return true;
  }
};
ModulePass *createAntiDebuggingPass() { return new AntiDebugging(); }
ModulePass *createAntiDebuggingPass(bool flag) {
  return new AntiDebugging(flag);
}
} // namespace llvm

char AntiDebugging::ID = 0;
INITIALIZE_PASS(AntiDebugging, "adb", "Enable AntiDebugging.", true, true)
