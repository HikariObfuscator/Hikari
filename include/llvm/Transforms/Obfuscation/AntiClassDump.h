#ifndef _ANTI_CLASSDUMP_H_
#define _ANTI_CLASSDUMP_H_
#include "llvm/Pass.h"
#include "llvm/IR/LegacyPassManager.h"
using namespace std;
using namespace llvm;

// Namespace
namespace llvm {
	ModulePass* createAntiClassDumpPass();
	void initializeAntiClassDumpPass(PassRegistry &Registry);
}
#endif
