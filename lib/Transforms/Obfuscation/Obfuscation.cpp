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
// End Obfuscator Options
namespace llvm {
struct Obfuscation : public ModulePass {
  static char ID;
  Obfuscation() : ModulePass(ID) {}
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
          P->doInitialization(M);
          P->runOnFunction(F);
          delete P;
        }
        if (EnableAllObfuscation || EnableBogusControlFlow) {
          P = createBogusControlFlowPass();
          P->doInitialization(M);
          P->runOnFunction(F);
          delete P;
        }
        if (EnableAllObfuscation || EnableFlattening) {
          P = createFlatteningPass();
          P->doInitialization(M);
          P->runOnFunction(F);
          delete P;
        }
        if (EnableAllObfuscation || EnableSubstitution) {
          P = createSubstitutionPass();
          P->runOnFunction(F);
          delete P;
        }
        if (EnableAllObfuscation || EnableIndirectBranching) {
          P = createIndirectBranchPass();
          P->runOnFunction(F);
          delete P;
        }
      }
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
INITIALIZE_PASS(Obfuscation, "obfus", "Enable Obfuscation", true, true)
