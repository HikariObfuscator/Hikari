#ifndef _OBFUSCATION_H_
#define _OBFUSCATION_H_
#include "llvm/Transforms/Obfuscation/AntiClassDump.h"
#include "llvm/Transforms/Obfuscation/Flattening.h"
#include "llvm/Transforms/Obfuscation/StringEncryption.h"
#include "llvm/Transforms/Obfuscation/AntiDebugging.h"
#include "llvm/Transforms/Obfuscation/FunctionCallObfuscate.h"
#include "llvm/Transforms/Obfuscation/Substitution.h"
#include "llvm/Transforms/Obfuscation/BogusControlFlow.h"
#include "llvm/Transforms/Obfuscation/Split.h"
#include "llvm/Transforms/Obfuscation/IndirectBranch.h"
#include "llvm/Transforms/Obfuscation/AntiHook.h"
#include "llvm/Transforms/Obfuscation/FunctionWrapper.h"
using namespace std;
using namespace llvm;

// Namespace
namespace llvm {
	ModulePass* createObfuscationPass();
	void initializeObfuscationPass(PassRegistry &Registry);
}

#endif
