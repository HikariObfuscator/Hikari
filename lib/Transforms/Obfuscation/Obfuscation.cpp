/*
    Copyright (C) 2017 Zhang(https://github.com/Naville/)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
  Hikari 's own "Pass Scheduler".
  Because currently there is no way to add dependency to transform passes
  Ref : http://lists.llvm.org/pipermail/llvm-dev/2011-February/038109.html
*/
#include "llvm/Transforms/Obfuscation/Obfuscation.h"
using namespace llvm;
using namespace std;
// Begin Obfuscator Options
static cl::opt<std::string> AesSeed("aesSeed", cl::init(""),
                                    cl::desc("seed for the AES-CTR PRNG"));
static cl::opt<bool> EnableAntiClassDump("enable-acdobf", cl::init(false),
                                         cl::NotHidden,
                                         cl::desc("Enable AntiClassDump."));
static cl::opt<bool>
    EnableBogusControlFlow("enable-bcfobf", cl::init(false), cl::NotHidden,
                           cl::desc("Enable BogusControlFlow."));
static cl::opt<bool> EnableFlattening("enable-cffobf", cl::init(false),
                                      cl::NotHidden,
                                      cl::desc("Enable Flattening."));
static cl::opt<bool>
    EnableBasicBlockSplit("enable-splitobf", cl::init(false), cl::NotHidden,
                          cl::desc("Enable BasicBlockSpliting."));
static cl::opt<bool>
    EnableSubstitution("enable-subobf", cl::init(false), cl::NotHidden,
                       cl::desc("Enable Instruction Substitution."));
static cl::opt<bool> EnableAllObfuscation("enable-allobf", cl::init(false),
                                          cl::NotHidden,
                                          cl::desc("Enable All Obfuscation."));
static cl::opt<bool> EnableAntiDebugging("enable-adb", cl::init(false),
                                         cl::NotHidden,
                                         cl::desc("Enable AntiDebugging."));
static cl::opt<bool> EnableFunctionCallObfuscate(
    "enable-fco", cl::init(false), cl::NotHidden,
    cl::desc("Enable Function CallSite Obfuscation."));
static cl::opt<bool>
    EnableStringEncryption("enable-strcry", cl::init(false), cl::NotHidden,
                           cl::desc("Enable Function CallSite Obfuscation."));
static cl::opt<bool>
    EnableIndirectBranching("enable-indibran", cl::init(false), cl::NotHidden,
                            cl::desc("Enable Indirect Branching."));

static cl::opt<bool> EnableAntiHooking("enable-antihook", cl::init(false),
                                       cl::NotHidden,
                                       cl::desc("Enable Indirect Branching."));
static cl::opt<bool>
    EnableFunctionWrapper("enable-funcwra", cl::init(false), cl::NotHidden,
                          cl::desc("Enable Function Wrapper."));
// End Obfuscator Options
namespace llvm {
struct Obfuscation : public ModulePass {
  static char ID;
  Obfuscation() : ModulePass(ID) {}
  StringRef getPassName() const override {
    return StringRef("HikariObfuscationScheduler");
  }
  bool runOnModule(Module &M) override {
    // Initial ACD Pass
    if (EnableAllObfuscation || EnableAntiClassDump) {
      ModulePass *P = createAntiClassDumpPass();
      P->doInitialization(M);
      P->runOnModule(M);
      delete P;
    }
    // Now do FCO
    if (EnableAllObfuscation || EnableFunctionCallObfuscate) {
      for (Module::iterator iter = M.begin(); iter != M.end(); iter++) {
        Function &F = *iter;
        if (!F.isDeclaration()) {
          FunctionPass *P = createFunctionCallObfuscatePass();
          P->doInitialization(M);
          P->runOnFunction(F);
          delete P;
        }
      }
    }
    if (EnableAllObfuscation || EnableAntiHooking) {
      ModulePass *P = createAntiHookPass();
      P->doInitialization(M);
      P->runOnModule(M);
      delete P;
    }
    if (EnableAllObfuscation || EnableAntiDebugging) {
      ModulePass *P = createAntiDebuggingPass();
      P->doInitialization(M);
      P->runOnModule(M);
      delete P;
    }
    if (EnableAllObfuscation || EnableStringEncryption) {
      // Now Encrypt Strings
      ModulePass *P = createStringEncryptionPass();
      P->runOnModule(M);
      delete P;
    }
    // Now perform Function-Level Obfuscation
    for (Module::iterator iter = M.begin(); iter != M.end(); iter++) {
      Function &F = *iter;
      if (!F.isDeclaration()) {
        FunctionPass *P = NULL;
        if (EnableAllObfuscation || EnableBasicBlockSplit) {
          P = createSplitBasicBlockPass();
          P->runOnFunction(F);
          delete P;
        }
        if (EnableAllObfuscation || EnableBogusControlFlow) {
          P = createBogusControlFlowPass();
          P->runOnFunction(F);
          delete P;
        }
        if (EnableAllObfuscation || EnableFlattening) {
          P = createFlatteningPass();
          P->runOnFunction(F);
          delete P;
        }
        if (EnableAllObfuscation || EnableSubstitution) {
          P = createSubstitutionPass();
          P->runOnFunction(F);
          delete P;
        }
      }
    }

    // Post-Run Clean-up part
    if (EnableAllObfuscation || EnableIndirectBranching) {
      FunctionPass *P = createIndirectBranchPass();
      P->doInitialization(M);
      vector<Function *> funcs;
      for (Module::iterator iter = M.begin(); iter != M.end(); iter++) {
        funcs.push_back(&*iter);
      }
      for (Function *F : funcs) {
        P->runOnFunction(*F);
      }
      delete P;
    }
    if (EnableAllObfuscation || EnableFunctionWrapper) {
      ModulePass *P = createFunctionWrapperPass();
      P->runOnModule(M);
      delete P;
    }
    return true;
  } // End runOnModule
};
ModulePass *createObfuscationPass() {
  if (!AesSeed.empty()) {
    llvm::cryptoutils->prng_seed(AesSeed.c_str());
  }
  return new Obfuscation();
}
} // namespace llvm
char Obfuscation::ID = 0;
INITIALIZE_PASS_BEGIN(Obfuscation, "obfus", "Enable Obfuscation", true, true)
INITIALIZE_PASS_DEPENDENCY(AntiClassDump);
INITIALIZE_PASS_DEPENDENCY(AntiDebugging);
INITIALIZE_PASS_DEPENDENCY(AntiHook);
INITIALIZE_PASS_DEPENDENCY(BogusControlFlow);
INITIALIZE_PASS_DEPENDENCY(Flattening);
INITIALIZE_PASS_DEPENDENCY(FunctionCallObfuscate);
INITIALIZE_PASS_DEPENDENCY(IndirectBranch);
INITIALIZE_PASS_DEPENDENCY(SplitBasicBlock);
INITIALIZE_PASS_DEPENDENCY(StringEncryption);
INITIALIZE_PASS_DEPENDENCY(Substitution);
INITIALIZE_PASS_END(Obfuscation, "obfus", "Enable Obfuscation", true, true)
