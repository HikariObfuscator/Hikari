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
      errs() << "Running AntiClassDump On " << M.getSourceFileName() << "\n";
      ModulePass *P = createAntiClassDumpPass();
      P->doInitialization(M);
      P->runOnModule(M);
      delete P;
    }
    // Now do FCO
    errs() << "Running FunctionCallObfuscate On " << M.getSourceFileName()
           << "\n";
    FunctionPass *FP = createFunctionCallObfuscatePass(
        EnableAllObfuscation || EnableFunctionCallObfuscate);
    FP->doInitialization(M);
    for (Module::iterator iter = M.begin(); iter != M.end(); iter++) {
      Function &F = *iter;
      if (!F.isDeclaration()) {
        FP->runOnFunction(F);
      }
    }
    delete FP;
    errs() << "Running AntiHooking On " << M.getSourceFileName() << "\n";
    ModulePass *MP =
        createAntiHookPass(EnableAllObfuscation || EnableAntiHooking);
    MP->doInitialization(M);
    MP->runOnModule(M);
    delete MP;
    errs() << "Running AntiDebugging On " << M.getSourceFileName() << "\n";
    MP = createAntiDebuggingPass(EnableAllObfuscation || EnableAntiDebugging);
    MP->doInitialization(M);
    MP->runOnModule(M);
    delete MP;
    // Now Encrypt Strings
    errs() << "Running StringEncryption On " << M.getSourceFileName() << "\n";
    MP = createStringEncryptionPass(EnableAllObfuscation ||
                                    EnableStringEncryption);
    MP->runOnModule(M);
    delete MP;
    /*
    // Placing FW here does provide the most obfuscation however the compile
    time
    // and product size would be literally unbearable for any large project
    // Move it to post run
    if (EnableAllObfuscation || EnableFunctionWrapper) {
      ModulePass *P = createFunctionWrapperPass();
      P->runOnModule(M);
      delete P;
    }*/
    // Now perform Function-Level Obfuscation
    for (Module::iterator iter = M.begin(); iter != M.end(); iter++) {
      Function &F = *iter;
      if (!F.isDeclaration()) {
        FunctionPass *P = NULL;
        errs() << "Running BasicBlockSplit On " << F.getName() << "\n";
        P = createSplitBasicBlockPass(EnableAllObfuscation ||
                                      EnableBasicBlockSplit);
        P->runOnFunction(F);
        delete P;
        errs() << "Running BogusControlFlow On " << F.getName() << "\n";
        P = createBogusControlFlowPass(EnableAllObfuscation ||
                                       EnableBogusControlFlow);
        P->runOnFunction(F);
        delete P;
        errs() << "Running ControlFlowFlattening On " << F.getName() << "\n";
        P = createFlatteningPass(EnableAllObfuscation || EnableFlattening);
        P->runOnFunction(F);
        delete P;
        errs() << "Running Instruction Substitution On " << F.getName() << "\n";
        P = createSubstitutionPass(EnableAllObfuscation || EnableSubstitution);
        P->runOnFunction(F);
        delete P;
      }
    }
    errs() << "Doing Post-Run Cleanup\n";
    errs() << "Running IndirectBranch On " << M.getSourceFileName() << "\n";
    FunctionPass *P = createIndirectBranchPass(EnableAllObfuscation ||
                                               EnableIndirectBranching);
    P->doInitialization(M);
    vector<Function *> funcs;
    for (Module::iterator iter = M.begin(); iter != M.end(); iter++) {
      funcs.push_back(&*iter);
    }
    for (Function *F : funcs) {
      P->runOnFunction(*F);
    }
    delete P;
    MP = createFunctionWrapperPass(EnableAllObfuscation ||
                                   EnableFunctionWrapper);
    MP->runOnModule(M);
    delete MP;
    // Cleanup Flags
    vector<Function *> toDelete;
    for (Module::iterator iter = M.begin(); iter != M.end(); iter++) {
      Function &F = *iter;
      if (F.isDeclaration() && F.hasName() &&
          F.getName().startswith("hikari_")) {
        for (User *U : F.users()) {
          if (Instruction *Inst = dyn_cast<Instruction>(U)) {
            Inst->eraseFromParent();
          }
        }
        toDelete.push_back(&F);
      }
    }
    for (Function *F : toDelete) {
      F->eraseFromParent();
    }
    errs() << "Hikari Out\n";
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
