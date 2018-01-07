/*
 *  LLVM SymbolObfuscation Pass
 *  https://github.com/Naville
 *  GPL V3 Licensed
 *  This is designed to be a LTO pass so we have a global view of all the TUs
 *
 */

#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/Obfuscation/SymbolObfuscation.h"
#include "llvm/Support/CommandLine.h"
#include <string>
#include <iostream>
#include <cstdlib>
using namespace llvm;
using namespace std;
static string obfcharacters="qwertyuiopasdfghjklzxcvbnm1234567890";
namespace llvm{
  struct SymbolObfuscation : public ModulePass {
    static char ID;
    SymbolObfuscation() : ModulePass(ID) {}
    StringRef getPassName()const override{return StringRef("SymbolObfuscation");}
    string randomString(int length){
      string name;
      name.resize(length);
      for(int i=0;i<length;i++){
        name[i]=obfcharacters[rand()%(obfcharacters.length()+1)];
      }
      return name;
    }
    bool runOnModule(Module &M) override {
      srand (time(NULL));
      //Normal Symbols
      for(Module::iterator Fun=M.begin();Fun!=M.end();Fun++){
        Function &F=*Fun;
        if (F.getName().str().compare("main")==0){
          errs()<<"Skipping main\n";
        }
        else if(F.empty()==false){
          //Rename
          errs()<<"Renaming Function: "<<F.getName()<<"\n";
          F.setName(randomString(16));
        }
        else{
          errs()<<"Skipping External Function: "<<F.getName()<<"\n";
        }
      }
      return true;
    }
  };
}
Pass *llvm::createSymbolObfuscationPass(){
  return new SymbolObfuscation();
}
char SymbolObfuscation::ID = 0;
INITIALIZE_PASS(SymbolObfuscation, "symobf", "Enable Symbol Obfuscation",true,true)
