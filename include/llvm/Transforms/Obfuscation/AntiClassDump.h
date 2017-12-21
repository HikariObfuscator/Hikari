#ifndef _ANTI_CLASSDUMP_H_
#define _ANTI_CLASSDUMP_H_
#include "llvm/Pass.h"
using namespace std;
using namespace llvm;

// Namespace
namespace llvm {
	void addAntiClassDumpPass(legacy::PassManagerBase &PM);
}
#endif
