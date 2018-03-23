/*
    LLVM BogusControlFlow Pass
    The main modification is the branching condition is calculated on-the-fly
    Instead of hard-code the always true condition. Relicensed from NCSA license
   to AGPL Copyright (C) 2017 Zhang(https://github.com/Naville/)

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

//===- BogusControlFlow.cpp - BogusControlFlow Obfuscation
// pass-------------------------===//
//
// This file implements BogusControlFlow's pass, inserting bogus control flow.
// It adds bogus flow to a given basic block this way:
//
// Before :
// 	         		     entry
//      			       |
//  	    	  	 ______v______
//   	    		|   Original  |
//   	    		|_____________|
//             		       |
// 		        	       v
//		        	     return
//
// After :
//           		     entry
//             		       |
//            		   ____v_____
//      			  |condition*| (false)
//           		  |__________|----+
//           		 (true)|          |
//             		       |          |
//           		 ______v______    |
// 		        +-->|   Original* |   |
// 		        |   |_____________| (true)
// 		        |   (false)|    !-----------> return
// 		        |    ______v______    |
// 		        |   |   Altered   |<--!
// 		        |   |_____________|
// 		        |__________|
//
//  * The results of these terminator's branch's conditions are always true, but
//  these predicates are
//    opacificated. For this, we declare two global values: x and y, and replace
//    the FCMP_TRUE predicate with (y < 10 || x * (x + 1) % 2 == 0) (this could
//    be improved, as the global values give a hint on where are the opaque
//    predicates)
//
//  The altered bloc is a copy of the original's one with junk instructions
//  added accordingly to the type of instructions we found in the bloc
//
//  Each basic block of the function is choosen if a random number in the range
//  [0,100] is smaller than the choosen probability rate. The default value
//  is 30. This value can be modify using the option -boguscf-prob=[value].
//  Value must be an integer in the range [0, 100], otherwise the default value
//  is taken. Exemple: -boguscf -boguscf-prob=60
//
//  The pass can also be loop many times on a function, including on the basic
//  blocks added in a previous loop. Be careful if you use a big probability
//  number and choose to run the loop many times wich may cause the pass to run
//  for a very long time. The default value is one loop, but you can change it
//  with -boguscf-loop=[value]. Value must be an integer greater than 1,
//  otherwise the default value is taken. Exemple: -boguscf -boguscf-loop=2
//
//
//  Defined debug types:
//  - "gen" : general informations
//  - "opt" : concerning the given options (parameter)
//  - "cfg" : printing the various function's cfg before transformation
//	      and after transformation if it has been modified, and all
//	      the functions at end of the pass, after doFinalization.
//
//  To use them all, simply use the -debug option.
//  To use only one of them, follow the pass' command by -debug-only=name.
//  Exemple, -boguscf -debug-only=cfg
//
//
//  Stats:
//  The following statistics will be printed if you use
//  the -stats command:
//
// a. Number of functions in this module
// b. Number of times we run on each function
// c. Initial number of basic blocks in this module
// d. Number of modified basic blocks
// e. Number of added basic blocks in this module
// f. Final number of basic blocks in this module
//
// file   : lib/Transforms/Obfuscation/BogusControlFlow.cpp
// date   : june 2012
// version: 1.0
// author : julie.michielin@gmail.com
// modifications: pjunod, Rinaldini Julien
// project: Obfuscator
// option : -boguscf
//
//===----------------------------------------------------------------------------------===//

#include "llvm/Transforms/Obfuscation/BogusControlFlow.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/NoFolder.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/Obfuscation/Utils.h"
#include "llvm/Transforms/Utils/Local.h"
#include <memory>

// Stats
#define DEBUG_TYPE "BogusControlFlow"
STATISTIC(NumFunction, "a. Number of functions in this module");
STATISTIC(NumTimesOnFunctions, "b. Number of times we run on each function");
STATISTIC(InitNumBasicBlocks,
          "c. Initial number of basic blocks in this module");
STATISTIC(NumModifiedBasicBlocks, "d. Number of modified basic blocks");
STATISTIC(NumAddedBasicBlocks,
          "e. Number of added basic blocks in this module");
STATISTIC(FinalNumBasicBlocks,
          "f. Final number of basic blocks in this module");

// Options for the pass
const int defaultObfRate = 70, defaultObfTime = 1;

static cl::opt<int>
    ObfProbRate("bcf_prob",
                cl::desc("Choose the probability [%] each basic blocks will be "
                         "obfuscated by the -bcf pass"),
                cl::value_desc("probability rate"), cl::init(defaultObfRate),
                cl::Optional);

static cl::opt<int>
    ObfTimes("bcf_loop",
             cl::desc("Choose how many time the -bcf pass loop on a function"),
             cl::value_desc("number of times"), cl::init(defaultObfTime),
             cl::Optional);
static cl::opt<int> ConditionExpressionComplexity(
    "bcf_cond_compl",
    cl::desc("The complexity of the expression used to generate branching "
             "condition"),
    cl::value_desc("Complexity"), cl::init(3), cl::Optional);
static Instruction::BinaryOps ops[] = {Instruction::Add, Instruction::Sub,
                                       Instruction::And, Instruction::Or,
                                       Instruction::Xor};
static CmpInst::Predicate preds[] = {CmpInst::ICMP_EQ,  CmpInst::ICMP_NE,
                                     CmpInst::ICMP_UGT, CmpInst::ICMP_UGE,
                                     CmpInst::ICMP_ULT, CmpInst::ICMP_ULE};
namespace {
  static bool OnlyUsedBy(Value *V, Value *Usr) {
    for (User *U : V->users())
      if (U != Usr)
        return false;

    return true;
  }
  static void RemoveDeadConstant(Constant *C) {
    assert(C->use_empty() && "Constant is not dead!");
    SmallPtrSet<Constant*, 4> Operands;
    for (Value *Op : C->operands())
      if (OnlyUsedBy(Op, C))
        Operands.insert(cast<Constant>(Op));
    if (GlobalVariable *GV = dyn_cast<GlobalVariable>(C)) {
      if (!GV->hasLocalLinkage()) return;   // Don't delete non-static globals.
      GV->eraseFromParent();
    }
    else if (!isa<Function>(C))
      if (isa<CompositeType>(C->getType()))
        C->destroyConstant();

    // If the constant referenced anything, see if we can delete it as well.
    for (Constant *O : Operands)
      RemoveDeadConstant(O);
  }
struct BogusControlFlow : public FunctionPass {
  static char ID; // Pass identification
  bool flag;
  BogusControlFlow() : FunctionPass(ID) { this->flag = true; }
  BogusControlFlow(bool flag) : FunctionPass(ID) { this->flag = flag; }
  /* runOnFunction
   *
   * Overwrite FunctionPass method to apply the transformation
   * to the function. See header for more details.
   */
  bool runOnFunction(Function &F) override {
    // Check if the percentage is correct
    if (ObfTimes <= 0) {
      errs() << "BogusControlFlow application number -bcf_loop=x must be x > 0";
      return false;
    }

    // Check if the number of applications is correct
    if (!((ObfProbRate > 0) && (ObfProbRate <= 100))) {
      errs() << "BogusControlFlow application basic blocks percentage "
                "-bcf_prob=x must be 0 < x <= 100";
      return false;
    }
    // If fla annotations
    if (toObfuscate(flag, &F, "bcf")) {
      errs() << "Running BogusControlFlow On " << F.getName() << "\n";
      bogus(F);
      doF(*F.getParent());
      return true;
    }

    return false;
  } // end of runOnFunction()

  void bogus(Function &F) {
    // For statistics and debug
    ++NumFunction;
    int NumBasicBlocks = 0;
    bool firstTime = true; // First time we do the loop in this function
    bool hasBeenModified = false;
    DEBUG_WITH_TYPE("opt", errs() << "bcf: Started on function " << F.getName()
                                  << "\n");
    DEBUG_WITH_TYPE("opt",
                    errs() << "bcf: Probability rate: " << ObfProbRate << "\n");
    if (ObfProbRate < 0 || ObfProbRate > 100) {
      DEBUG_WITH_TYPE("opt", errs()
                                 << "bcf: Incorrect value,"
                                 << " probability rate set to default value: "
                                 << defaultObfRate << " \n");
      ObfProbRate = defaultObfRate;
    }
    DEBUG_WITH_TYPE("opt", errs()
                               << "bcf: How many times: " << ObfTimes << "\n");
    if (ObfTimes <= 0) {
      DEBUG_WITH_TYPE("opt", errs()
                                 << "bcf: Incorrect value,"
                                 << " must be greater than 1. Set to default: "
                                 << defaultObfTime << " \n");
      ObfTimes = defaultObfTime;
    }
    NumTimesOnFunctions = ObfTimes;
    int NumObfTimes = ObfTimes;

    // Real begining of the pass
    // Loop for the number of time we run the pass on the function
    do {
      DEBUG_WITH_TYPE("cfg", errs() << "bcf: Function " << F.getName()
                                    << ", before the pass:\n");
      DEBUG_WITH_TYPE("cfg", F.viewCFG());
      // Put all the function's block in a list
      std::list<BasicBlock *> basicBlocks;
      for (Function::iterator i = F.begin(); i != F.end(); ++i) {
        BasicBlock *BB = &*i;
        if (!BB->isEHPad() && !BB->isLandingPad()) {
          basicBlocks.push_back(BB);
        }
      }
      DEBUG_WITH_TYPE(
          "gen", errs() << "bcf: Iterating on the Function's Basic Blocks\n");

      while (!basicBlocks.empty()) {
        NumBasicBlocks++;
        // Basic Blocks' selection
        if ((int)llvm::cryptoutils->get_range(100) <= ObfProbRate) {
          DEBUG_WITH_TYPE("opt", errs() << "bcf: Block " << NumBasicBlocks
                                        << " selected. \n");
          hasBeenModified = true;
          ++NumModifiedBasicBlocks;
          NumAddedBasicBlocks += 3;
          FinalNumBasicBlocks += 3;
          // Add bogus flow to the given Basic Block (see description)
          BasicBlock *basicBlock = basicBlocks.front();
          addBogusFlow(basicBlock, F);
        } else {
          DEBUG_WITH_TYPE("opt", errs() << "bcf: Block " << NumBasicBlocks
                                        << " not selected.\n");
        }
        // remove the block from the list
        basicBlocks.pop_front();

        if (firstTime) { // first time we iterate on this function
          ++InitNumBasicBlocks;
          ++FinalNumBasicBlocks;
        }
      } // end of while(!basicBlocks.empty())
      DEBUG_WITH_TYPE("gen",
                      errs() << "bcf: End of function " << F.getName() << "\n");
      if (hasBeenModified) { // if the function has been modified
        DEBUG_WITH_TYPE("cfg", errs() << "bcf: Function " << F.getName()
                                      << ", after the pass: \n");
        DEBUG_WITH_TYPE("cfg", F.viewCFG());
      } else {
        DEBUG_WITH_TYPE("cfg", errs()
                                   << "bcf: Function's not been modified \n");
      }
      firstTime = false;
    } while (--NumObfTimes > 0);
  }

  /* addBogusFlow
   *
   * Add bogus flow to a given basic block, according to the header's
   * description
   */
  virtual void addBogusFlow(BasicBlock *basicBlock, Function &F) {

    // Split the block: first part with only the phi nodes and debug info and
    // terminator
    //                  created by splitBasicBlock. (-> No instruction)
    //                  Second part with every instructions from the original
    //                  block
    // We do this way, so we don't have to adjust all the phi nodes, metadatas
    // and so on for the first block. We have to let the phi nodes in the first
    // part, because they actually are updated in the second part according to
    // them.
    BasicBlock::iterator i1 = basicBlock->begin();
    if (basicBlock->getFirstNonPHIOrDbgOrLifetime())
      i1 = (BasicBlock::iterator)basicBlock->getFirstNonPHIOrDbgOrLifetime();
    Twine *var;
    var = new Twine("originalBB");
    BasicBlock *originalBB = basicBlock->splitBasicBlock(i1, *var);
    DEBUG_WITH_TYPE("gen", errs()
                               << "bcf: First and original basic blocks: ok\n");

    // Creating the altered basic block on which the first basicBlock will jump
    Twine *var3 = new Twine("alteredBB");
    BasicBlock *alteredBB = createAlteredBasicBlock(originalBB, *var3, &F);
    DEBUG_WITH_TYPE("gen", errs() << "bcf: Altered basic block: ok\n");

    // Now that all the blocks are created,
    // we modify the terminators to adjust the control flow.

    alteredBB->getTerminator()->eraseFromParent();
    basicBlock->getTerminator()->eraseFromParent();
    DEBUG_WITH_TYPE("gen", errs() << "bcf: Terminator removed from the altered"
                                  << " and first basic blocks\n");

    // Preparing a condition..
    // For now, the condition is an always true comparaison between 2 float
    // This will be complicated after the pass (in doFinalization())
    Value *LHS = ConstantFP::get(Type::getFloatTy(F.getContext()), 1.0);
    Value *RHS = ConstantFP::get(Type::getFloatTy(F.getContext()), 1.0);
    DEBUG_WITH_TYPE("gen", errs() << "bcf: Value LHS and RHS created\n");

    // The always true condition. End of the first block
    Twine *var4 = new Twine("condition");
    FCmpInst *condition =
        new FCmpInst(*basicBlock, FCmpInst::FCMP_TRUE, LHS, RHS, *var4);
    DEBUG_WITH_TYPE("gen", errs() << "bcf: Always true condition created\n");

    // Jump to the original basic block if the condition is true or
    // to the altered block if false.
    BranchInst::Create(originalBB, alteredBB, (Value *)condition, basicBlock);
    DEBUG_WITH_TYPE(
        "gen",
        errs() << "bcf: Terminator instruction in first basic block: ok\n");

    // The altered block loop back on the original one.
    BranchInst::Create(originalBB, alteredBB);
    DEBUG_WITH_TYPE(
        "gen", errs() << "bcf: Terminator instruction in altered block: ok\n");

    // The end of the originalBB is modified to give the impression that
    // sometimes it continues in the loop, and sometimes it return the desired
    // value (of course it's always true, so it always use the original
    // terminator..
    //  but this will be obfuscated too;) )

    // iterate on instruction just before the terminator of the originalBB
    BasicBlock::iterator i = originalBB->end();

    // Split at this point (we only want the terminator in the second part)
    Twine *var5 = new Twine("originalBBpart2");
    BasicBlock *originalBBpart2 = originalBB->splitBasicBlock(--i, *var5);
    DEBUG_WITH_TYPE("gen",
                    errs() << "bcf: Terminator part of the original basic block"
                           << " is isolated\n");
    // the first part go either on the return statement or on the begining
    // of the altered block.. So we erase the terminator created when splitting.
    originalBB->getTerminator()->eraseFromParent();
    // We add at the end a new always true condition
    Twine *var6 = new Twine("condition2");
    FCmpInst *condition2 =
        new FCmpInst(*originalBB, CmpInst::FCMP_TRUE, LHS, RHS, *var6);
    // BranchInst::Create(originalBBpart2, alteredBB, (Value
    // *)condition2,originalBB);  Do random behavior to avoid pattern
    // recognition This is achieved by jumping to a random BB
    switch (llvm::cryptoutils->get_uint16_t() % 2) {
    case 0: {
      BranchInst::Create(originalBBpart2, originalBB, condition2, originalBB);
      break;
    }
    case 1: {
      BranchInst::Create(originalBBpart2, alteredBB, condition2, originalBB);
      break;
    }
    default: {
      BranchInst::Create(originalBBpart2, originalBB, condition2, originalBB);
      break;
    }
    }
    DEBUG_WITH_TYPE("gen", errs()
                               << "bcf: Terminator original basic block: ok\n");
    DEBUG_WITH_TYPE("gen", errs() << "bcf: End of addBogusFlow().\n");

  } // end of addBogusFlow()

  /* createAlteredBasicBlock
   *
   * This function return a basic block similar to a given one.
   * It's inserted just after the given basic block.
   * The instructions are similar but junk instructions are added between
   * the cloned one. The cloned instructions' phi nodes, metadatas, uses and
   * debug locations are adjusted to fit in the cloned basic block and
   * behave nicely.
   */
  virtual BasicBlock *createAlteredBasicBlock(BasicBlock *basicBlock,
                                              const Twine &Name = "gen",
                                              Function *F = 0) {
    // Useful to remap the informations concerning instructions.
    ValueToValueMapTy VMap;
    BasicBlock *alteredBB = llvm::CloneBasicBlock(basicBlock, VMap, Name, F);
    DEBUG_WITH_TYPE("gen", errs() << "bcf: Original basic block cloned\n");
    // Remap operands.
    BasicBlock::iterator ji = basicBlock->begin();
    for (BasicBlock::iterator i = alteredBB->begin(), e = alteredBB->end();
         i != e; ++i) {
      // Loop over the operands of the instruction
      for (User::op_iterator opi = i->op_begin(), ope = i->op_end(); opi != ope;
           ++opi) {
        // get the value for the operand
        Value *v = MapValue(*opi, VMap, RF_None, 0);
        if (v != 0) {
          *opi = v;
          DEBUG_WITH_TYPE("gen",
                          errs() << "bcf: Value's operand has been setted\n");
        }
      }
      DEBUG_WITH_TYPE("gen", errs() << "bcf: Operands remapped\n");
      // Remap phi nodes' incoming blocks.
      if (PHINode *pn = dyn_cast<PHINode>(i)) {
        for (unsigned j = 0, e = pn->getNumIncomingValues(); j != e; ++j) {
          Value *v = MapValue(pn->getIncomingBlock(j), VMap, RF_None, 0);
          if (v != 0) {
            pn->setIncomingBlock(j, cast<BasicBlock>(v));
          }
        }
      }
      DEBUG_WITH_TYPE("gen", errs() << "bcf: PHINodes remapped\n");
      // Remap attached metadata.
      SmallVector<std::pair<unsigned, MDNode *>, 4> MDs;
      i->getAllMetadata(MDs);
      DEBUG_WITH_TYPE("gen", errs() << "bcf: Metadatas remapped\n");
      // important for compiling with DWARF, using option -g.
      i->setDebugLoc(ji->getDebugLoc());
      ji++;
      DEBUG_WITH_TYPE("gen", errs()
                                 << "bcf: Debug information location setted\n");

    } // The instructions' informations are now all correct

    DEBUG_WITH_TYPE("gen",
                    errs() << "bcf: The cloned basic block is now correct\n");
    DEBUG_WITH_TYPE(
        "gen",
        errs() << "bcf: Starting to add junk code in the cloned bloc...\n");

    // add random instruction in the middle of the bloc. This part can be
    // improve
    for (BasicBlock::iterator i = alteredBB->begin(), e = alteredBB->end();
         i != e; ++i) {
      // in the case we find binary operator, we modify slightly this part by
      // randomly insert some instructions
      if (i->isBinaryOp()) { // binary instructions
        unsigned opcode = i->getOpcode();
        BinaryOperator *op, *op1 = NULL;
        Twine *var = new Twine("_");
        // treat differently float or int
        // Binary int
        if (opcode == Instruction::Add || opcode == Instruction::Sub ||
            opcode == Instruction::Mul || opcode == Instruction::UDiv ||
            opcode == Instruction::SDiv || opcode == Instruction::URem ||
            opcode == Instruction::SRem || opcode == Instruction::Shl ||
            opcode == Instruction::LShr || opcode == Instruction::AShr ||
            opcode == Instruction::And || opcode == Instruction::Or ||
            opcode == Instruction::Xor) {
          for (int random = (int)llvm::cryptoutils->get_range(10); random < 10;
               ++random) {
            switch (llvm::cryptoutils->get_range(4)) { // to improve
            case 0:                                    // do nothing
              break;
            case 1:
              op = BinaryOperator::CreateNeg(i->getOperand(0), *var, &*i);
              op1 = BinaryOperator::Create(Instruction::Add, op,
                                           i->getOperand(1), "gen", &*i);
              break;
            case 2:
              op1 = BinaryOperator::Create(Instruction::Sub, i->getOperand(0),
                                           i->getOperand(1), *var, &*i);
              op = BinaryOperator::Create(Instruction::Mul, op1,
                                          i->getOperand(1), "gen", &*i);
              break;
            case 3:
              op = BinaryOperator::Create(Instruction::Shl, i->getOperand(0),
                                          i->getOperand(1), *var, &*i);
              break;
            }
          }
        }
        // Binary float
        if (opcode == Instruction::FAdd || opcode == Instruction::FSub ||
            opcode == Instruction::FMul || opcode == Instruction::FDiv ||
            opcode == Instruction::FRem) {
          for (int random = (int)llvm::cryptoutils->get_range(10); random < 10;
               ++random) {
            switch (llvm::cryptoutils->get_range(3)) { // can be improved
            case 0:                                    // do nothing
              break;
            case 1:
              op = BinaryOperator::CreateFNeg(i->getOperand(0), *var, &*i);
              op1 = BinaryOperator::Create(Instruction::FAdd, op,
                                           i->getOperand(1), "gen", &*i);
              break;
            case 2:
              op = BinaryOperator::Create(Instruction::FSub, i->getOperand(0),
                                          i->getOperand(1), *var, &*i);
              op1 = BinaryOperator::Create(Instruction::FMul, op,
                                           i->getOperand(1), "gen", &*i);
              break;
            }
          }
        }
        if (opcode == Instruction::ICmp) { // Condition (with int)
          ICmpInst *currentI = (ICmpInst *)(&i);
          switch (llvm::cryptoutils->get_range(3)) { // must be improved
          case 0:                                    // do nothing
            break;
          case 1:
            currentI->swapOperands();
            break;
          case 2: // randomly change the predicate
            switch (llvm::cryptoutils->get_range(10)) {
            case 0:
              currentI->setPredicate(ICmpInst::ICMP_EQ);
              break; // equal
            case 1:
              currentI->setPredicate(ICmpInst::ICMP_NE);
              break; // not equal
            case 2:
              currentI->setPredicate(ICmpInst::ICMP_UGT);
              break; // unsigned greater than
            case 3:
              currentI->setPredicate(ICmpInst::ICMP_UGE);
              break; // unsigned greater or equal
            case 4:
              currentI->setPredicate(ICmpInst::ICMP_ULT);
              break; // unsigned less than
            case 5:
              currentI->setPredicate(ICmpInst::ICMP_ULE);
              break; // unsigned less or equal
            case 6:
              currentI->setPredicate(ICmpInst::ICMP_SGT);
              break; // signed greater than
            case 7:
              currentI->setPredicate(ICmpInst::ICMP_SGE);
              break; // signed greater or equal
            case 8:
              currentI->setPredicate(ICmpInst::ICMP_SLT);
              break; // signed less than
            case 9:
              currentI->setPredicate(ICmpInst::ICMP_SLE);
              break; // signed less or equal
            }
            break;
          }
        }
        if (opcode == Instruction::FCmp) { // Conditions (with float)
          FCmpInst *currentI = (FCmpInst *)(&i);
          switch (llvm::cryptoutils->get_range(3)) { // must be improved
          case 0:                                    // do nothing
            break;
          case 1:
            currentI->swapOperands();
            break;
          case 2: // randomly change the predicate
            switch (llvm::cryptoutils->get_range(10)) {
            case 0:
              currentI->setPredicate(FCmpInst::FCMP_OEQ);
              break; // ordered and equal
            case 1:
              currentI->setPredicate(FCmpInst::FCMP_ONE);
              break; // ordered and operands are unequal
            case 2:
              currentI->setPredicate(FCmpInst::FCMP_UGT);
              break; // unordered or greater than
            case 3:
              currentI->setPredicate(FCmpInst::FCMP_UGE);
              break; // unordered, or greater than, or equal
            case 4:
              currentI->setPredicate(FCmpInst::FCMP_ULT);
              break; // unordered or less than
            case 5:
              currentI->setPredicate(FCmpInst::FCMP_ULE);
              break; // unordered, or less than, or equal
            case 6:
              currentI->setPredicate(FCmpInst::FCMP_OGT);
              break; // ordered and greater than
            case 7:
              currentI->setPredicate(FCmpInst::FCMP_OGE);
              break; // ordered and greater than or equal
            case 8:
              currentI->setPredicate(FCmpInst::FCMP_OLT);
              break; // ordered and less than
            case 9:
              currentI->setPredicate(FCmpInst::FCMP_OLE);
              break; // ordered or less than, or equal
            }
            break;
          }
        }
      }
    }
    // Remove DIs from AlterBB
    vector<CallInst *> toRemove;
    vector<Constant*> DeadConstants;
    for (Instruction &I : *alteredBB) {
      if (CallInst *CI = dyn_cast<CallInst>(&I)) {
        if (CI->getCalledFunction() != nullptr &&
            CI->getCalledFunction()->getName().startswith("llvm.dbg")) {
          toRemove.push_back(CI);
        }
      }
    }
    // Shamefully stolen from IPO/StripSymbols.cpp
    for (CallInst *CI : toRemove) {
      Value *Arg1 = CI->getArgOperand(0);
      Value *Arg2 = CI->getArgOperand(1);
      assert(CI->use_empty() && "llvm.dbg intrinsic should have void result");
      CI->eraseFromParent();
      if (Arg1->use_empty()) {
        if (Constant *C = dyn_cast<Constant>(Arg1)) {
          DeadConstants.push_back(C);
        } else {
          RecursivelyDeleteTriviallyDeadInstructions(Arg1);
        }
      }
      if (Arg2->use_empty()) {
        if (Constant *C = dyn_cast<Constant>(Arg2)) {
          DeadConstants.push_back(C);
        }
      }
    }
    while (!DeadConstants.empty()) {
      Constant *C = DeadConstants.back();
      DeadConstants.pop_back();
      if (GlobalVariable *GV = dyn_cast<GlobalVariable>(C)) {
        if (GV->hasLocalLinkage())
          RemoveDeadConstant(GV);
      } else
        RemoveDeadConstant(C);
    }
    return alteredBB;
  } // end of createAlteredBasicBlock()

  /* doFinalization
   *
   * Overwrite FunctionPass method to apply the transformations to the whole
   * module. This part obfuscate all the always true predicates of the module.
   * More precisely, the condition which predicate is FCMP_TRUE.
   * It also remove all the functions' basic blocks' and instructions' names.
   */
  bool doF(Module &M) {
    // In this part we extract all always-true predicate and replace them with
    // opaque predicate: For this, we declare two global values: x and y, and
    // replace the FCMP_TRUE predicate with (y < 10 || x * (x + 1) % 2 == 0) A
    // better way to obfuscate the predicates would be welcome. In the meantime
    // we will erase the name of the basic blocks, the instructions and the
    // functions.
    DEBUG_WITH_TYPE("gen", errs() << "bcf: Starting doFinalization...\n");
    std::vector<Instruction *> toEdit, toDelete;
    // BinaryOperator *op, *op1 = NULL;
    // ICmpInst *condition, *condition2;
    // Looking for the conditions and branches to transform
    for (Module::iterator mi = M.begin(), me = M.end(); mi != me; ++mi) {
      for (Function::iterator fi = mi->begin(), fe = mi->end(); fi != fe;
           ++fi) {
        // fi->setName("");
        TerminatorInst *tbb = fi->getTerminator();
        if (tbb->getOpcode() == Instruction::Br) {
          BranchInst *br = (BranchInst *)(tbb);
          if (br->isConditional()) {
            FCmpInst *cond = (FCmpInst *)br->getCondition();
            unsigned opcode = cond->getOpcode();
            if (opcode == Instruction::FCmp) {
              if (cond->getPredicate() == FCmpInst::FCMP_TRUE) {
                DEBUG_WITH_TYPE("gen",
                                errs() << "bcf: an always true predicate !\n");
                toDelete.push_back(cond); // The condition
                toEdit.push_back(tbb);    // The branch using the condition
              }
            }
          }
        }
        /*
        for (BasicBlock::iterator bi = fi->begin(), be = fi->end() ; bi != be;
        ++bi){ bi->setName(""); // setting the basic blocks' names
        }
        */
      }
    }
    // Replacing all the branches we found
    for (std::vector<Instruction *>::iterator i = toEdit.begin();
         i != toEdit.end(); ++i) {
      // Previously We Use LLVM EE To Calculate LHS and RHS
      // Since IRBuilder<> uses ConstantFolding to fold constants.
      // The return instruction is already returning constants
      // The variable names below are the artifact from the Emulation Era
      Type *I32Ty = Type::getInt32Ty(M.getContext());
      Module emuModule("HikariBCFEmulator", M.getContext());
      emuModule.setDataLayout(M.getDataLayout());
      emuModule.setTargetTriple(M.getTargetTriple());
      Function *emuFunction =
          Function::Create(FunctionType::get(I32Ty, false),
                           GlobalValue::LinkageTypes::PrivateLinkage,
                           "BeginExecution", &emuModule);
      BasicBlock *EntryBlock =
          BasicBlock::Create(M.getContext(), "", emuFunction);

      // BasicBlock* RealEntryBlock=&((*i)->getFunction()->getEntryBlock());
      IRBuilder<> IRBReal(*i);
      IRBuilder<> IRBEmu(EntryBlock);
      // First,Construct a real RHS that will be used in the actual condition
      Constant *RealRHS = ConstantInt::get(I32Ty, cryptoutils->get_uint32_t());
      // Prepare Initial LHS and RHS to bootstrap the emulator
      Constant *LHSC = ConstantInt::get(I32Ty, cryptoutils->get_uint32_t());
      Constant *RHSC = ConstantInt::get(I32Ty, cryptoutils->get_uint32_t());
      GlobalVariable *LHSGV =
          new GlobalVariable(M, Type::getInt32Ty(M.getContext()), false,
                             GlobalValue::PrivateLinkage, LHSC, "LHSGV");
      GlobalVariable *RHSGV =
          new GlobalVariable(M, Type::getInt32Ty(M.getContext()), false,
                             GlobalValue::PrivateLinkage, RHSC, "RHSGV");
      LoadInst *LHS = IRBReal.CreateLoad(LHSGV, "Initial LHS");
      LoadInst *RHS = IRBReal.CreateLoad(RHSGV, "Initial LHS");
      // To Speed-Up Evaluation
      Value *emuLHS = LHSC;
      Value *emuRHS = RHSC;
      Instruction::BinaryOps initialOp = ops[llvm::cryptoutils->get_uint32_t() %
                                             (sizeof(ops) / sizeof(ops[0]))];
      Value *emuLast =
          IRBEmu.CreateBinOp(initialOp, emuLHS, emuRHS, "EmuInitialCondition");
      Value *Last =
          IRBReal.CreateBinOp(initialOp, LHS, RHS, "InitialCondition");
      for (int i = 0; i < ConditionExpressionComplexity; i++) {
        Constant *newTmp = ConstantInt::get(I32Ty, cryptoutils->get_uint32_t());
        Instruction::BinaryOps initialOp =
            ops[llvm::cryptoutils->get_uint32_t() %
                (sizeof(ops) / sizeof(ops[0]))];
        emuLast = IRBEmu.CreateBinOp(initialOp, emuLast, newTmp,
                                     "EmuInitialCondition");
        Last = IRBReal.CreateBinOp(initialOp, Last, newTmp, "InitialCondition");
      }
      // Randomly Generate Predicate
      CmpInst::Predicate pred = preds[llvm::cryptoutils->get_uint32_t() %
                                      (sizeof(preds) / sizeof(preds[0]))];
      Last = IRBReal.CreateICmp(pred, Last, RealRHS);
      emuLast = IRBEmu.CreateICmp(pred, emuLast, RealRHS);
      ReturnInst *RI = IRBEmu.CreateRet(emuLast);
      ConstantInt *emuCI = cast<ConstantInt>(RI->getReturnValue());
      uint64_t emulateResult = emuCI->getZExtValue();
      vector<BasicBlock *> BBs; // Start To Prepare IndirectBranching
      if (emulateResult == 1) {
        // Our ConstantExpr evaluates to true;

        BranchInst::Create(((BranchInst *)*i)->getSuccessor(0),
                           ((BranchInst *)*i)->getSuccessor(1), (Value *)Last,
                           ((BranchInst *)*i)->getParent());
      } else {
        // False, swap operands

        BranchInst::Create(((BranchInst *)*i)->getSuccessor(1),
                           ((BranchInst *)*i)->getSuccessor(0), (Value *)Last,
                           ((BranchInst *)*i)->getParent());
      }
      EntryBlock->eraseFromParent();
      emuFunction->eraseFromParent();
      DEBUG_WITH_TYPE("gen", errs() << "bcf: Erase branch instruction:"
                                    << *((BranchInst *)*i) << "\n");
      (*i)->eraseFromParent(); // erase the branch
    }
    // Erase all the associated conditions we found
    for (std::vector<Instruction *>::iterator i = toDelete.begin();
         i != toDelete.end(); ++i) {
      DEBUG_WITH_TYPE("gen", errs() << "bcf: Erase condition instruction:"
                                    << *((Instruction *)*i) << "\n");
      (*i)->eraseFromParent();
    }

    // Only for debug
    DEBUG_WITH_TYPE("cfg", errs() << "bcf: End of the pass, here are the "
                                     "graphs after doFinalization\n");
    for (Module::iterator mi = M.begin(), me = M.end(); mi != me; ++mi) {
      DEBUG_WITH_TYPE("cfg", errs()
                                 << "bcf: Function " << mi->getName() << "\n");
      DEBUG_WITH_TYPE("cfg", mi->viewCFG());
    }

    return true;
  } // end of doFinalization
};  // end of struct BogusControlFlow : public FunctionPass
} // namespace

char BogusControlFlow::ID = 0;
INITIALIZE_PASS(BogusControlFlow, "bcfobf", "Enable BogusControlFlow.", true,
                true)
FunctionPass *llvm::createBogusControlFlowPass() {
  return new BogusControlFlow();
}
FunctionPass *llvm::createBogusControlFlowPass(bool flag) {
  return new BogusControlFlow(flag);
}
