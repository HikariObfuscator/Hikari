#ifndef __UTILS_OBF__
#define __UTILS_OBF__

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Transforms/Utils/Local.h" // For DemoteRegToStack and DemotePHIToStack
#include <stdio.h>
#include <sstream>
#include <map>
#include <set>
using namespace std;
using namespace llvm;

void fixStack(Function *f);
std::string readAnnotate(Function *f);
map<GlobalValue*,StringRef> BuildAnnotateMap(Module& M);
bool toObfuscate(bool flag, Function *f, std::string attribute);
void FixBasicBlockConstantExpr(BasicBlock *BB);
void FixFunctionConstantExpr(Function *Func);
void appendToAnnotations(Module &M,ConstantStruct *Data);
#endif
