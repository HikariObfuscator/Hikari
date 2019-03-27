#ifndef _FUNCTION_CALL_OBFUSCATION_H_
#define _FUNCTION_CALL_OBFUSCATION_H_
#include "llvm/Pass.h"
#include "llvm/IR/LegacyPassManager.h"
using namespace std;
using namespace llvm;

// Namespace
namespace llvm {
	FunctionPass* createFunctionCallObfuscatePass();
	FunctionPass* createFunctionCallObfuscatePass(bool flag);
	void initializeFunctionCallObfuscatePass(PassRegistry &Registry);
}
#endif
