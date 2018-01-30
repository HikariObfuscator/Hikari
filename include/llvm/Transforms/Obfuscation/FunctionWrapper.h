#ifndef _FUNCTION_WRAPPER_H_
#define _FUNCTION_WRAPPER_H_
#include "llvm/Pass.h"
using namespace std;
using namespace llvm;

// Namespace
namespace llvm {
	ModulePass* createFunctionWrapperPass();
	void initializeFunctionWrapperPass(PassRegistry &Registry);
}
#endif
