#include "llvm/Transforms/Obfuscation/Utils.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/NoFolder.h"
// Shamefully borrowed from ../Scalar/RegToMem.cpp :(
bool valueEscapes(Instruction *Inst) {
  BasicBlock *BB = Inst->getParent();
  for (Value::use_iterator UI = Inst->use_begin(), E = Inst->use_end(); UI != E;
       ++UI) {
    Instruction *I = cast<Instruction>(*UI);
    if (I->getParent() != BB || isa<PHINode>(I)) {
      return true;
    }
  }
  return false;
}
void appendToAnnotations(Module &M, ConstantStruct *Data) {
  // Type for the annotation array
  // { i8*, i8*, i8*, i32 }
  GlobalVariable *AnnotationGV = M.getGlobalVariable("llvm.global.annotations");
  Type *Int8PtrTy = Type::getInt8PtrTy(M.getContext());
  Type *Int32Ty = Type::getInt32Ty(M.getContext());
  if (AnnotationGV == nullptr) {
    ArrayType *AT = ArrayType::get(Data->getType(), 1);
    ConstantArray *CA = cast<ConstantArray>(ConstantArray::get(AT, {Data}));
    GlobalVariable *newGV = new GlobalVariable(M, CA->getType(), false,
                                               GlobalValue::AppendingLinkage,
                                               CA, "llvm.global.annotations");
    newGV->setSection("llvm.metadata");
    return;
  } else {
    StructType *ST = StructType::get(
        M.getContext(), {Int8PtrTy, Int8PtrTy, Int8PtrTy, Int32Ty});
    vector<Constant *> exists;
    for (unsigned i = 0;
         i < cast<ArrayType>(AnnotationGV->getInitializer()->getType())
                 ->getNumElements();
         i++) {
      exists.push_back(AnnotationGV->getInitializer()->getAggregateElement(i));
    }
    exists.push_back(Data);
    ArrayType *AT = ArrayType::get(ST, exists.size());
    ConstantArray *CA = cast<ConstantArray>(
        ConstantArray::get(AT, ArrayRef<Constant *>(exists)));
    GlobalVariable *newGV = new GlobalVariable(M, CA->getType(), false,
                                               GlobalValue::AppendingLinkage,
                                               CA, "llvm.global.annotations");
    newGV->setSection("llvm.metadata");
    return;
  }
}
void FixFunctionConstantExpr(Function *Func) {
  // Replace ConstantExpr with equal instructions
  // Otherwise replacing on Constant will crash the compiler
  for (BasicBlock &BB : *Func) {
    FixBasicBlockConstantExpr(&BB);
  }
}
void FixBasicBlockConstantExpr(BasicBlock *BB) {
  // Replace ConstantExpr with equal instructions
  // Otherwise replacing on Constant will crash the compiler
  // Things to note:
  // - Phis must be placed at BB start so CEs must be placed prior to current BB
  assert(!BB->empty()&&"BasicBlock is empty!");
  assert((BB->getParent()!=NULL)&&"BasicBlock must be in a Function!");
  Instruction* FunctionInsertPt=&*(BB->getParent()->getEntryBlock().getFirstInsertionPt());
  //Instruction* LocalBBInsertPt=&*(BB.getFirstInsertionPt());

  for (Instruction &I : *BB) {
    if(isa<LandingPadInst>(I)||isa<FuncletPadInst>(I)){
      continue;
    }
    for(unsigned i=0;i<I.getNumOperands();i++){
      if(ConstantExpr* C=dyn_cast<ConstantExpr>(I.getOperand(i))){
        Instruction* InsertPt=&I;
        IRBuilder<NoFolder> IRB(InsertPt);
        if(isa<PHINode>(I)){
          IRB.SetInsertPoint(FunctionInsertPt);
        }
        Instruction *Inst = IRB.Insert(C->getAsInstruction());
        I.setOperand(i,Inst);
      }
    }
  }
}


map<GlobalValue *, StringRef> BuildAnnotateMap(Module &M) {
  map<GlobalValue *, StringRef> VAMap;
  GlobalVariable *glob = M.getGlobalVariable("llvm.global.annotations");
  if (glob != nullptr && glob->hasInitializer()) {
    ConstantArray *CDA = cast<ConstantArray>(glob->getInitializer());
    for (Value *op : CDA->operands()) {
      ConstantStruct *anStruct = cast<ConstantStruct>(op);
      /*
        Structure: [Value,Annotation,SourceFilePath,LineNumber]
        Usually wrapped inside GEP/BitCast
        We only care about Value and Annotation Here
      */
      GlobalValue *Value =
          cast<GlobalValue>(anStruct->getOperand(0)->getOperand(0));
      GlobalVariable *Annotation =
          cast<GlobalVariable>(anStruct->getOperand(1)->getOperand(0));
      if (Annotation->hasInitializer()) {
        VAMap[Value] =
            cast<ConstantDataSequential>(Annotation->getInitializer())
                ->getAsCString();
      }
    }
  }
  return VAMap;
}

void fixStack(Function *f) {
  // Try to remove phi node and demote reg to stack
  std::vector<PHINode *> tmpPhi;
  std::vector<Instruction *> tmpReg;
  BasicBlock *bbEntry = &*f->begin();

  do {
    tmpPhi.clear();
    tmpReg.clear();

    for (Function::iterator i = f->begin(); i != f->end(); ++i) {

      for (BasicBlock::iterator j = i->begin(); j != i->end(); ++j) {

        if (isa<PHINode>(j)) {
          PHINode *phi = cast<PHINode>(j);
          tmpPhi.push_back(phi);
          continue;
        }
        if (!(isa<AllocaInst>(j) && j->getParent() == bbEntry) &&
            (valueEscapes(&*j) || j->isUsedOutsideOfBlock(&*i))) {
          tmpReg.push_back(&*j);
          continue;
        }
      }
    }
    for (unsigned int i = 0; i != tmpReg.size(); ++i) {
      DemoteRegToStack(*tmpReg.at(i), f->begin()->getTerminator());
    }

    for (unsigned int i = 0; i != tmpPhi.size(); ++i) {
      DemotePHIToStack(tmpPhi.at(i), f->begin()->getTerminator());
    }

  } while (tmpReg.size() != 0 || tmpPhi.size() != 0);
}

std::string readAnnotate(Function *f) {
  std::string annotation = "";

  // Get annotation variable
  GlobalVariable *glob =
      f->getParent()->getGlobalVariable("llvm.global.annotations");

  if (glob != NULL) {
    // Get the array
    if (ConstantArray *ca = dyn_cast<ConstantArray>(glob->getInitializer())) {
      for (unsigned i = 0; i < ca->getNumOperands(); ++i) {
        // Get the struct
        if (ConstantStruct *structAn =
                dyn_cast<ConstantStruct>(ca->getOperand(i))) {
          if (ConstantExpr *expr =
                  dyn_cast<ConstantExpr>(structAn->getOperand(0))) {
            // If it's a bitcast we can check if the annotation is concerning
            // the current function
            if (expr->getOpcode() == Instruction::BitCast &&
                expr->getOperand(0) == f) {
              ConstantExpr *note = cast<ConstantExpr>(structAn->getOperand(1));
              // If it's a GetElementPtr, that means we found the variable
              // containing the annotations
              if (note->getOpcode() == Instruction::GetElementPtr) {
                if (GlobalVariable *annoteStr =
                        dyn_cast<GlobalVariable>(note->getOperand(0))) {
                  if (ConstantDataSequential *data =
                          dyn_cast<ConstantDataSequential>(
                              annoteStr->getInitializer())) {
                    if (data->isString()) {
                      annotation += data->getAsString().lower() + " ";
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  return annotation;
}

// Unlike O-LLVM which uses __attribute__ that is not supported by the ObjC CFE.
// We use a dummy call here and remove the call later
// Very dumb and definitely slower than the function attribute method
// Merely a hack
bool readFlag(Function *f, std::string attribute) {
  for (inst_iterator I = inst_begin(f); I != inst_end(f); I++) {
    Instruction *Inst = &*I;
    if (CallInst *CI = dyn_cast<CallInst>(Inst)) {
      if (CI->getCalledFunction() != nullptr &&
          CI->getCalledFunction()->getName().contains("hikari_" + attribute)) {
        CI->eraseFromParent();
        return true;
      }
    }
  }
  return false;
}
bool toObfuscate(bool flag, Function *f, std::string attribute) {

  // Check if declaration
  if (f->isDeclaration()) {
    return false;
  }
  // Check external linkage
  if (f->hasAvailableExternallyLinkage() != 0) {
    return false;
  }
  std::string attr = attribute;
  std::string attrNo = "no" + attr;
  // We have to check the nofla flag first
  // Because .find("fla") is true for a string like "fla" or
  // "nofla"
  if (readAnnotate(f).find(attrNo) != std::string::npos ||
      readFlag(f, attrNo)) {
    return false;
  }
  if (readAnnotate(f).find(attr) != std::string::npos || readFlag(f, attr)) {
    return true;
  }
  if (flag == true) {
    return true;
  }
  return false;
}
