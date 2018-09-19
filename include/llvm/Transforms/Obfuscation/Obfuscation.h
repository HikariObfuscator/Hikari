#ifndef _OBFUSCATION_H_
#define _OBFUSCATION_H_
#include "llvm/Transforms/Obfuscation/AntiClassDump.h"
#include "llvm/Transforms/Obfuscation/Flattening.h"
#include "llvm/Transforms/Obfuscation/StringEncryption.h"
#include "llvm/Transforms/Obfuscation/FunctionCallObfuscate.h"
#include "llvm/Transforms/Obfuscation/Substitution.h"
#include "llvm/Transforms/Obfuscation/BogusControlFlow.h"
#include "llvm/Transforms/Obfuscation/Split.h"
#include "llvm/Transforms/Obfuscation/IndirectBranch.h"
#include "llvm/Transforms/Obfuscation/FunctionWrapper.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/NoFolder.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/Transforms/Obfuscation/CryptoUtils.h"
#include "llvm/Transforms/Scalar.h"
#if __has_include("llvm/Transforms/Utils.h")
#include "llvm/Transforms/Utils.h"
#endif
using namespace std;
using namespace llvm;

// Namespace
namespace llvm {
	ModulePass* createObfuscationPass();
	void initializeObfuscationPass(PassRegistry &Registry);
}

#endif
