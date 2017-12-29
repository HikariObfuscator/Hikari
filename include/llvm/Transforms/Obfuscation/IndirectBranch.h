#ifndef _INDIRECT_BRACH_H_
#define _INDIRECT_BRACH_H_
#include "llvm/Pass.h"
using namespace std;
using namespace llvm;

// Namespace
namespace llvm {
	Pass* createIndirectBranchPass();
}
#endif
