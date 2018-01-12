/*
     LLVM SymbolObfuscation Pass
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

     This is designed to be a LTO pass so we have a global view of all the TUs
 */

#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"
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
ModulePass *llvm::createSymbolObfuscationPass(){
  return new SymbolObfuscation();
}
char SymbolObfuscation::ID = 0;
INITIALIZE_PASS(SymbolObfuscation, "symobf", "Enable Symbol Obfuscation",true,true)
