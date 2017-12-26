#ifndef _STRING_ENCRYPTION_H_
#define _STRING_ENCRYPTION_H_
#include "llvm/Pass.h"
#include "llvm/IR/LegacyPassManager.h"
using namespace std;
using namespace llvm;

// Namespace
namespace llvm {
	Pass* createStringEncryptionPass();
}
#endif
