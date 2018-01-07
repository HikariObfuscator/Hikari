#ifndef _FUNCTION_CALL_OBFUSCATION_H_
#define _FUNCTION_CALL_OBFUSCATION_H_
#include "llvm/Pass.h"
#include "llvm/IR/LegacyPassManager.h"
using namespace std;
using namespace llvm;

// Namespace
namespace llvm {
	Pass* createFunctionCallObfuscatePass();
	void initializeFunctionCallObfuscatePass(PassRegistry &Registry);
}
#endif
