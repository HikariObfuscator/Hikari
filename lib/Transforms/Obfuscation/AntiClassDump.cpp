/*
 *  LLVM AntiClassDump Pass
 *  Note we only aim to support Darwin ObjC. GNUStep and other implementations
 *  are not considered
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
  For maximum usability. We provide two modes for this pass, as defined in
  llvm/Transforms/Obfuscation/AntiClassDump.h THIN mode is used on per-module
  basis without LTO overhead and structs are left in the module where possible.
  This is particularly useful for cases where LTO is not possible. For example
  static library. Full mode is used at LTO stage, this mode constructs
  dependency graph and perform full wipe-out as well as llvm.global_ctors
  injection.
  This pass only provides thin mode
*/

#include "llvm/Transforms/Obfuscation/AntiClassDump.h"
#include "llvm/ADT/Triple.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <string>
using namespace llvm;
using namespace std;
static cl::opt<bool>
    UseInitialize("acd-use-initialize", cl::init(true), cl::NotHidden,
                  cl::desc("[AntiClassDump]Inject codes to +initialize"));
namespace llvm {
struct AntiClassDump : public ModulePass {
  static char ID;
  AntiClassDump() : ModulePass(ID) {}
  StringRef getPassName() const override { return StringRef("AntiClassDump"); }
  virtual bool doInitialization(Module &M) override {
    // Basic Defs
    Triple tri(M.getTargetTriple());
    if (tri.getVendor() != Triple::VendorType::Apple) {
      // We only support AAPL's ObjC Implementation ATM
      errs()
          << M.getTargetTriple()
          << " is Not Supported For LLVM AntiClassDump\nProbably GNU Step?\n";
      return false;
    }
    Type *Int64Ty = Type::getInt64Ty(M.getContext());
    Type *Int32Ty = Type::getInt32Ty(M.getContext());
    Type *Int8PtrTy = Type::getInt8PtrTy(M.getContext());
    Type *Int8Ty = Type::getInt8Ty(M.getContext());
    // Generic ObjC Runtime Declarations
    FunctionType *IMPType =
        FunctionType::get(Int8PtrTy, {Int8PtrTy, Int8PtrTy}, true);
    PointerType *IMPPointerType = PointerType::get(IMPType, 0);
    vector<Type *> classReplaceMethodTypeArgs;
    classReplaceMethodTypeArgs.push_back(Int8PtrTy);
    classReplaceMethodTypeArgs.push_back(Int8PtrTy);
    classReplaceMethodTypeArgs.push_back(IMPPointerType);
    classReplaceMethodTypeArgs.push_back(Int8PtrTy);
    FunctionType *class_replaceMethod_type =
        FunctionType::get(IMPPointerType, classReplaceMethodTypeArgs, false);
    M.getOrInsertFunction("class_replaceMethod", class_replaceMethod_type);
    FunctionType *sel_registerName_type =
        FunctionType::get(Int8PtrTy, {Int8PtrTy}, false);
    M.getOrInsertFunction("sel_registerName", sel_registerName_type);
    FunctionType *objc_getClass_type =
        FunctionType::get(Int8PtrTy, {Int8PtrTy}, false);
    M.getOrInsertFunction("objc_getClass", objc_getClass_type);
    M.getOrInsertFunction("objc_getMetaClass", objc_getClass_type);
    StructType *objc_property_attribute_t_type = reinterpret_cast<StructType *>(
        M.getTypeByName("struct.objc_property_attribute_t"));
    if (objc_property_attribute_t_type == NULL) {
      vector<Type *> types;
      types.push_back(Int8PtrTy);
      types.push_back(Int8PtrTy);
      objc_property_attribute_t_type = StructType::create(
          ArrayRef<Type *>(types), "struct.objc_property_attribute_t");
      M.getOrInsertGlobal("struct.objc_property_attribute_t",
                          objc_property_attribute_t_type);
    }
    vector<Type *> allocaClsTypeVector;
    vector<Type *> addIvarTypeVector;
    vector<Type *> addPropTypeVector;
    allocaClsTypeVector.push_back(Int8PtrTy);
    allocaClsTypeVector.push_back(Int8PtrTy);
    addIvarTypeVector.push_back(Int8PtrTy);
    addIvarTypeVector.push_back(Int8PtrTy);
    addPropTypeVector.push_back(Int8PtrTy);
    addPropTypeVector.push_back(Int8PtrTy);
    addPropTypeVector.push_back(objc_property_attribute_t_type->getPointerTo());
    if (tri.isArch64Bit()) {
      // We are 64Bit Device
      allocaClsTypeVector.push_back(Int64Ty);
      addIvarTypeVector.push_back(Int64Ty);
      addPropTypeVector.push_back(Int64Ty);
    } else {
      // Not 64Bit.However we are still on apple platform.So We are
      // ARMV7/ARMV7S/i386
      // PowerPC is ignored, feel free to open a PR if you want to
      allocaClsTypeVector.push_back(Int32Ty);
      addIvarTypeVector.push_back(Int32Ty);
      addPropTypeVector.push_back(Int32Ty);
    }
    addIvarTypeVector.push_back(Int8Ty);
    addIvarTypeVector.push_back(Int8PtrTy);
    // Types Collected. Now Inject Functions
    FunctionType *addIvarType =
        FunctionType::get(Int8Ty, ArrayRef<Type *>(addIvarTypeVector), false);
    M.getOrInsertFunction("class_addIvar", addIvarType);
    FunctionType *addPropType =
        FunctionType::get(Int8Ty, ArrayRef<Type *>(addPropTypeVector), false);
    M.getOrInsertFunction("class_addProperty", addPropType);
    FunctionType *class_getName_Type =
        FunctionType::get(Int8PtrTy, {Int8PtrTy}, false);
    M.getOrInsertFunction("class_getName", class_getName_Type);
    FunctionType *objc_getMetaClass_Type =
        FunctionType::get(Int8PtrTy, {Int8PtrTy}, false);
    M.getOrInsertFunction("objc_getMetaClass", objc_getMetaClass_Type);
    return true;
  }
  bool runOnModule(Module &M) override {
    errs() << "Running AntiClassDump On " << M.getSourceFileName() << "\n";
    GlobalVariable *OLCGV = M.getGlobalVariable("OBJC_LABEL_CLASS_$", true);
    if (OLCGV == NULL) {
      errs() << "No ObjC Class Found in :" << M.getSourceFileName() << "\n";
      // No ObjC class found.
      return false;
    }
    /*// Create our own Initializer
    FunctionType *InitializerType = FunctionType::get(
        Type::getVoidTy(M.getContext()), ArrayRef<Type *>(), false);
    Function *Initializer = Function::Create(
        InitializerType, GlobalValue::LinkageTypes::PrivateLinkage, "", &M);
    BasicBlock *EntryBB = BasicBlock::Create(M.getContext(), "", Initializer);
    //
    IRBuilder<> IRB(EntryBB);
    IRB.CreateRetVoid();*/
    assert(OLCGV->hasInitializer() &&
           "OBJC_LABEL_CLASS_$ Doesn't Have Initializer.");
    ConstantArray *OBJC_LABEL_CLASS_CDS =
        dyn_cast<ConstantArray>(OLCGV->getInitializer());

    assert(OBJC_LABEL_CLASS_CDS &&
           "OBJC_LABEL_CLASS_$ Not ConstantArray.Is the target using "
           "unsupported legacy runtime?");
    vector<string> readyclses; // This is for storing classes that can be used
                               // in handleClass()
    deque<string> tmpclses;    // This is temporary storage for classes
    map<string /*class*/, string /*super class*/> dependency;
    map<string /*Class*/, GlobalVariable *>
        GVMapping; // Map ClassName to corresponding GV
    for (unsigned i = 0; i < OBJC_LABEL_CLASS_CDS->getNumOperands(); i++) {
      ConstantExpr *clsEXPR =
          dyn_cast<ConstantExpr>(OBJC_LABEL_CLASS_CDS->getOperand(i));
      GlobalVariable *CEGV = dyn_cast<GlobalVariable>(clsEXPR->getOperand(0));
      ConstantStruct *clsCS = dyn_cast<ConstantStruct>(CEGV->getInitializer());
      /*
        First Operand MetaClass.
        Second Operand SuperClass
        Fifth Operand ClassRO
      */
      GlobalVariable *SuperClassGV =
          (clsCS->getOperand(1) == NULL)
              ? NULL
              : dyn_cast<GlobalVariable>(clsCS->getOperand(1));
      string supclsName = "";
      string clsName = CEGV->getName().str();
      clsName.replace(clsName.find("OBJC_CLASS_$_"), strlen("OBJC_CLASS_$_"),
                      "");

      if (SuperClassGV !=
          NULL) { // We need to handle Classed that doesn't have a base
        supclsName = SuperClassGV->getName().str();
        supclsName.replace(supclsName.find("OBJC_CLASS_$_"),
                           strlen("OBJC_CLASS_$_"), "");
      }
      dependency[clsName] = supclsName;
      GVMapping[clsName] = CEGV;
      if (supclsName == "" /*NULL Super Class*/ ||
          (SuperClassGV != NULL &&
           !SuperClassGV->hasInitializer() /*External Super Class*/)) {
        readyclses.push_back(clsName);
      } else {
        tmpclses.push_back(clsName);
      }
    }
    // Sort Initialize Sequence Based On Dependency
    while (tmpclses.size() > 0) {
      string clstmp = tmpclses.front();
      tmpclses.pop_front();
      string SuperClassName = dependency[clstmp];
      if (SuperClassName != "" &&
          std::find(readyclses.begin(), readyclses.end(), SuperClassName) ==
              readyclses.end()) {
        // SuperClass is unintialized non-null class.Push back and waiting until
        // baseclass is allocated
        tmpclses.push_back(clstmp);
      } else {
        // BaseClass Ready. Push into ReadyClasses
        readyclses.push_back(clstmp);
      }
    }

    // Now run handleClass for each class
    for (string className : readyclses) {
      handleClass(GVMapping[className], &M);
    }
    return true;
  } // runOnModule
  map<string, Value *>
  splitclass_ro_t(ConstantStruct *class_ro,
                  Module *M) { // Split a class_ro_t structure
    map<string, Value *> info;
    StructType *objc_method_list_t_type =
        M->getTypeByName("struct.__method_list_t");
    StructType *ivar_list_t_type = M->getTypeByName("struct._ivar_list_t");
    StructType *property_list_t_type = M->getTypeByName("struct._prop_list_t");
    for (unsigned i = 0; i < class_ro->getType()->getNumElements(); i++) {
      Constant *tmp = dyn_cast<Constant>(class_ro->getAggregateElement(i));
      if (tmp->isNullValue()) {
        continue;
      }
      Type *type = tmp->getType();
      if (type == ivar_list_t_type->getPointerTo()) {
        info["IVARLIST"] = cast<ConstantExpr>(tmp);
      } else if (type == property_list_t_type->getPointerTo()) {
        info["PROPLIST"] = cast<ConstantExpr>(tmp);
      } else if (type == objc_method_list_t_type->getPointerTo()) {
        // Insert Methods
        ConstantExpr *methodListCE = cast<ConstantExpr>(tmp);
        // Note:methodListCE is also a BitCastConstantExpr
        GlobalVariable *methodListGV =
            cast<GlobalVariable>(methodListCE->getOperand(0));
        // Now BitCast is stripped out.
        assert(methodListGV->hasInitializer() &&
               "MethodListGV doesn't have initializer");
        ConstantStruct *methodListStruct =
            cast<ConstantStruct>(methodListGV->getInitializer());
        // Extracting %struct._objc_method array from %struct.__method_list_t =
        // type { i32, i32, [0 x %struct._objc_method] }
        info["METHODLIST"] =
            cast<ConstantArray>(methodListStruct->getOperand(2));
      }
    }
    return info;
  } // splitclass_ro_t
  void handleClass(GlobalVariable *GV, Module *M) {
    assert(GV->hasInitializer() &&
           "ObjC Class Structure's Initializer Missing");
    ConstantStruct *CS = dyn_cast<ConstantStruct>(GV->getInitializer());
    StringRef ClassName = GV->getName();
    ClassName = ClassName.substr(strlen("OBJC_CLASS_$_"));
    StringRef SuperClassName = CS->getOperand(1)->getName();
    SuperClassName = SuperClassName.substr(strlen("OBJC_CLASS_$_"));
    errs() << "Handling Class:" << ClassName
           << " With SuperClass:" << SuperClassName << "\n";

    // Let's extract stuffs
    // struct _class_t {
    //   struct _class_t *isa;
    //   struct _class_t * const superclass;
    //   void *cache;
    //   IMP *vtable;
    //   struct class_ro_t *ro;
    // }
    GlobalVariable *metaclassGV = cast<GlobalVariable>(CS->getOperand(0));
    GlobalVariable *class_ro = cast<GlobalVariable>(CS->getOperand(4));
    assert(metaclassGV->hasInitializer() && "MetaClass GV Initializer Missing");
    GlobalVariable *metaclass_ro =
        cast<GlobalVariable>(metaclassGV->getInitializer()->getOperand(
            metaclassGV->getInitializer()->getNumOperands() - 1));
    IRBuilder<> *IRB = NULL;
    // Begin IRBuilder Initializing
    map<string, Value *> Info = splitclass_ro_t(
        cast<ConstantStruct>(metaclass_ro->getInitializer()), M);
    BasicBlock *EntryBB = NULL;
    if (Info.find("METHODLIST") != Info.end()) {
      ConstantArray *method_list = cast<ConstantArray>(Info["METHODLIST"]);
      for (unsigned i = 0; i < method_list->getNumOperands(); i++) {
        ConstantStruct *methodStruct =
            cast<ConstantStruct>(method_list->getOperand(i));
        // methodStruct has type %struct._objc_method = type { i8*, i8*, i8* }
        // which contains {GEP(NAME),GEP(TYPE),BitCast(IMP)}
        // Let's extract these info now
        // methodStruct->getOperand(0)->getOperand(0) is SELName
        GlobalVariable *SELNameGV =
            cast<GlobalVariable>(methodStruct->getOperand(0)->getOperand(0));
        ConstantDataSequential *SELNameCDS =
            cast<ConstantDataSequential>(SELNameGV->getInitializer());
        StringRef selname = SELNameCDS->getAsCString();
        if ((selname == StringRef("initialize") && UseInitialize) ||
            (selname == StringRef("load") && !UseInitialize)) {
          Function *IMPFunc =
              cast<Function>(methodStruct->getOperand(2)->getOperand(0));
          errs() << "Found Existing initializer\n";
          EntryBB = &(IMPFunc->getEntryBlock());
        }
      }
    } else {
      errs() << "Didn't Find ClassMethod List\n";
    }
    bool NeedTerminator = false;
    if (EntryBB == NULL) {
      NeedTerminator = true;
      // We failed to find existing +initializer,create new one
      errs() << "Creating initializer\n";
      FunctionType *InitializerType = FunctionType::get(
          Type::getVoidTy(M->getContext()), ArrayRef<Type *>(), false);
      Function *Initializer = Function::Create(
          InitializerType, GlobalValue::LinkageTypes::PrivateLinkage,
          "AntiClassDumpInitializer", M);
      EntryBB = BasicBlock::Create(M->getContext(), "", Initializer);
    }
    if (NeedTerminator) {
      IRBuilder<> foo(EntryBB);
      foo.CreateRetVoid();
    }
    IRB = new IRBuilder<>(EntryBB, EntryBB->getFirstInsertionPt());
    // End IRBuilder Initializing
    // We now prepare ObjC API Definitions
    Function *objc_getClass = M->getFunction("objc_getClass");
    // Type *Int8PtrTy = Type::getInt8PtrTy(M->getContext());
    // End of ObjC API Definitions
    Value *ClassNameGV = IRB->CreateGlobalStringPtr(ClassName);
    // Now Scan For Props and Ivars in OBJC_CLASS_RO AND OBJC_METACLASS_RO
    // Note that class_ro_t's structure is different for 32 and 64bit runtime
    CallInst *Class = IRB->CreateCall(objc_getClass, {ClassNameGV});
    // Add Methods
    ConstantStruct *metaclassCS =
        cast<ConstantStruct>(class_ro->getInitializer());
    ConstantStruct *classCS =
        cast<ConstantStruct>(metaclass_ro->getInitializer());
    if (!metaclassCS->getAggregateElement(5)->isNullValue()) {
      errs() << "Handling Instance Methods For Class:" << ClassName << "\n";
      HandleMethods(metaclassCS, IRB, M, Class, false);

      errs() << "Updating Class Method Map For Class:" << ClassName << "\n";
      Type *objc_method_type = M->getTypeByName("struct._objc_method");
      ArrayType *AT = ArrayType::get(objc_method_type, 0);
      Constant *newMethodList = ConstantArray::get(AT, ArrayRef<Constant *>());
      GlobalVariable *methodListGV =
          cast<GlobalVariable>(metaclassCS->getAggregateElement(5)->getOperand(
              0)); // is striped MethodListGV
      StructType *oldGVType =
          cast<StructType>(methodListGV->getInitializer()->getType());
      vector<Type *> newStructType;
      vector<Constant *> newStructValue;
      // I'm fully aware that it's consistent Int32 on all platforms
      // This is future-proof
      newStructType.push_back(oldGVType->getElementType(0));
      newStructValue.push_back(
          methodListGV->getInitializer()->getAggregateElement(0u));
      newStructType.push_back(oldGVType->getElementType(1));
      newStructValue.push_back(
          ConstantInt::get(oldGVType->getElementType(1), 0));
      newStructType.push_back(AT);
      newStructValue.push_back(newMethodList);
      StructType *newType =
          StructType::get(M->getContext(), ArrayRef<Type *>(newStructType));
      Constant *newMethodStruct = ConstantStruct::get(
          newType,
          ArrayRef<Constant *>(newStructValue)); // l_OBJC_$_CLASS_METHODS_
      GlobalVariable *newMethodStructGV = new GlobalVariable(
          *M, newType, true, GlobalValue::LinkageTypes::PrivateLinkage,
          newMethodStruct);
      newMethodStructGV->copyAttributesFrom(methodListGV);
      Constant *bitcastExpr = ConstantExpr::getBitCast(
          newMethodStructGV,
          M->getTypeByName("struct.__method_list_t")->getPointerTo());
      metaclassCS->handleOperandChange(metaclassCS->getAggregateElement(5),
                                       bitcastExpr);
      GlobalVariable *metadatacompilerusedGV = cast<GlobalVariable>(
          M->getGlobalVariable("llvm.compiler.used", true));
      ConstantArray *metadatacompilerusedlist =
          cast<ConstantArray>(metadatacompilerusedGV->getInitializer());
      ArrayType *oldmetadatatype = metadatacompilerusedlist->getType();
      vector<Constant *> values;
      for (unsigned i = 0; i < metadatacompilerusedlist->getNumOperands();
           i++) {
        Constant *foo =
            metadatacompilerusedlist->getOperand(i)->stripPointerCasts();
        if (foo != methodListGV) {
          values.push_back(metadatacompilerusedlist->getOperand(i));
        }
      }
      // values.push_back(ConstantExpr::getBitCast(newMethodStructGV,Type::getInt8PtrTy(M->getContext())));
      ArrayType *newmetadatatype =
          ArrayType::get(oldmetadatatype->getElementType(), values.size());
      Constant *newused =
          ConstantArray::get(newmetadatatype, ArrayRef<Constant *>(values));
      metadatacompilerusedGV->dropAllReferences();
      metadatacompilerusedGV->removeFromParent();
      methodListGV->dropAllReferences();
      methodListGV->eraseFromParent();
      GlobalVariable *newIntializer =
          new GlobalVariable(*M, newmetadatatype, true,
                             GlobalValue::LinkageTypes::AppendingLinkage,
                             newused, "llvm.compiler.used");
      newIntializer->copyAttributesFrom(metadatacompilerusedGV);
      metadatacompilerusedGV->deleteValue();
      errs() << "Updated Instance Method Map of:" << class_ro->getName()
             << "\n";
    }
    // MethodList has index of 5
    // We need to create a new type first then bitcast to required type later
    // Since the original type's contained arraytype has count of 0
    GlobalVariable *methodListGV = NULL; // is striped MethodListGV
    if (!classCS->getAggregateElement(5)->isNullValue()) {
      errs() << "Handling Class Methods For Class:" << ClassName << "\n";
      HandleMethods(classCS, IRB, M, Class, true);
      methodListGV =
          cast<GlobalVariable>(classCS->getAggregateElement(5)->getOperand(0));
    }
    errs() << "Updating Class Method Map For Class:" << ClassName << "\n";
    Type *objc_method_type = M->getTypeByName("struct._objc_method");
    ArrayType *AT = ArrayType::get(objc_method_type, 1);
    Constant *MethName = NULL;
    if (UseInitialize) {
      MethName = cast<Constant>(IRB->CreateGlobalStringPtr("initialize"));
    } else {
      MethName = cast<Constant>(IRB->CreateGlobalStringPtr("load"));
    }
    // This method signature is generated by clang
    // See
    // http://llvm.org/viewvc/llvm-project/cfe/trunk/lib/AST/ASTContext.cpp?revision=320954&view=markup
    // ASTContext::getObjCEncodingForMethodDecl
    // The one hard-coded here is generated for macOS 64Bit
    Triple tri = Triple(M->getTargetTriple());
    Constant *MethType = NULL;
    if (tri.isOSDarwin() && tri.isArch64Bit()) {
      MethType = cast<Constant>(IRB->CreateGlobalStringPtr("v16@0:8"));
    } else if (tri.isOSDarwin() && tri.isArch32Bit()) {
      MethType = cast<Constant>(IRB->CreateGlobalStringPtr("v8@0:4"));
    } else {
      errs() << "Unknown Platform.Blindly applying method signature for "
                "macOS 64Bit\n";
      MethType = cast<Constant>(IRB->CreateGlobalStringPtr("v16@0:8"));
    }
    Constant *BitCastedIMP = cast<Constant>(
        IRB->CreateBitCast(IRB->GetInsertBlock()->getParent(),
                           objc_getClass->getFunctionType()->getParamType(0)));
    vector<Constant *> methodStructContents; //{GEP(NAME),GEP(TYPE),IMP}
    methodStructContents.push_back(MethName);
    methodStructContents.push_back(MethType);
    methodStructContents.push_back(BitCastedIMP);
    Constant *newMethod = ConstantStruct::get(
        cast<StructType>(objc_method_type),
        ArrayRef<Constant *>(methodStructContents)); // objc_method_t
    Constant *newMethodList = ConstantArray::get(
        AT, ArrayRef<Constant *>(newMethod)); // Container of objc_method_t
    vector<Type *> newStructType;
    vector<Constant *> newStructValue;
    // I'm fully aware that it's consistent Int32 on all platforms
    // This is future-proof
    newStructType.push_back(Type::getInt32Ty(M->getContext()));
    newStructValue.push_back(ConstantInt::get(Type::getInt32Ty(M->getContext()),
                                              0x18)); // 0x18 is extracted from
                                                      // built-code on macOS.No
                                                      // idea what does it mean
    newStructType.push_back(Type::getInt32Ty(M->getContext()));
    newStructValue.push_back(ConstantInt::get(Type::getInt32Ty(M->getContext()),
                                              1)); // this is class count
    newStructType.push_back(AT);
    newStructValue.push_back(newMethodList);
    StructType *newType =
        StructType::get(M->getContext(), ArrayRef<Type *>(newStructType));
    Constant *newMethodStruct = ConstantStruct::get(
        newType,
        ArrayRef<Constant *>(newStructValue)); // l_OBJC_$_CLASS_METHODS_
    GlobalVariable *newMethodStructGV = new GlobalVariable(
        *M, newType, true, GlobalValue::LinkageTypes::PrivateLinkage,
        newMethodStruct);
    if (methodListGV) {
      newMethodStructGV->copyAttributesFrom(methodListGV);
    }
    Constant *bitcastExpr = ConstantExpr::getBitCast(
        newMethodStructGV,
        M->getTypeByName("struct.__method_list_t")->getPointerTo());
    classCS->handleOperandChange(classCS->getAggregateElement(5), bitcastExpr);
    GlobalVariable *metadatacompilerusedGV =
        cast<GlobalVariable>(M->getGlobalVariable("llvm.compiler.used", true));
    ConstantArray *metadatacompilerusedlist =
        cast<ConstantArray>(metadatacompilerusedGV->getInitializer());
    ArrayType *oldmetadatatype = metadatacompilerusedlist->getType();
    vector<Constant *> values;
    for (unsigned i = 0; i < metadatacompilerusedlist->getNumOperands(); i++) {
      Constant *foo =
          metadatacompilerusedlist->getOperand(i)->stripPointerCasts();
      if (foo != methodListGV) {
        values.push_back(metadatacompilerusedlist->getOperand(i));
      }
    }
    ArrayType *newmetadatatype =
        ArrayType::get(oldmetadatatype->getElementType(), values.size());
    Constant *newused =
        ConstantArray::get(newmetadatatype, ArrayRef<Constant *>(values));
    metadatacompilerusedGV->dropAllReferences();
    metadatacompilerusedGV->removeFromParent();
    if (methodListGV) {
      methodListGV->dropAllReferences();
      methodListGV->eraseFromParent();
    }
    GlobalVariable *newIntializer = new GlobalVariable(
        *M, newmetadatatype, true, GlobalValue::LinkageTypes::AppendingLinkage,
        newused, "llvm.compiler.used");
    newIntializer->copyAttributesFrom(metadatacompilerusedGV);
    metadatacompilerusedGV->deleteValue();
    errs() << "Updated Class Method Map of:" << class_ro->getName() << "\n";
    // End ClassCS Handling
  } // handleClass
  void HandleMethods(ConstantStruct *class_ro, IRBuilder<> *IRB, Module *M,
                     Value *Class, bool isMetaClass) {
    Function *sel_registerName = M->getFunction("sel_registerName");
    Function *class_replaceMethod = M->getFunction("class_replaceMethod");
    Function *class_getName = M->getFunction("class_getName");
    Function *objc_getMetaClass = M->getFunction("objc_getMetaClass");
    StructType *objc_method_list_t_type =
        M->getTypeByName("struct.__method_list_t");
    for (unsigned i = 0; i < class_ro->getType()->getNumElements(); i++) {
      Constant *tmp = dyn_cast<Constant>(class_ro->getAggregateElement(i));
      if (tmp->isNullValue()) {
        continue;
      }
      Type *type = tmp->getType();
      if (type == objc_method_list_t_type->getPointerTo()) {
        // Insert Methods
        ConstantExpr *methodListCE = cast<ConstantExpr>(tmp);
        // Note:methodListCE is also a BitCastConstantExpr
        GlobalVariable *methodListGV =
            dyn_cast<GlobalVariable>(methodListCE->getOperand(0));
        // Now BitCast is stripped out.
        assert(methodListGV->hasInitializer() &&
               "MethodListGV doesn't have initializer");
        ConstantStruct *methodListStruct =
            cast<ConstantStruct>(methodListGV->getInitializer());
        // Extracting %struct._objc_method array from %struct.__method_list_t =
        // type { i32, i32, [0 x %struct._objc_method] }
        ConstantArray *methodList =
            cast<ConstantArray>(methodListStruct->getOperand(2));
        for (unsigned i = 0; i < methodList->getNumOperands(); i++) {
          ConstantStruct *methodStruct =
              cast<ConstantStruct>(methodList->getOperand(i));
          // methodStruct has type %struct._objc_method = type { i8*, i8*, i8* }
          // which contains {GEP(NAME),GEP(TYPE),IMP}
          // Let's extract these info now
          // We should first register the selector
          CallInst *SEL =
              IRB->CreateCall(sel_registerName, {methodStruct->getOperand(0)});
          Type *IMPType =
              class_replaceMethod->getFunctionType()->getParamType(2);
          Value *BitCastedIMP =
              IRB->CreateBitCast(methodStruct->getOperand(2), IMPType);
          vector<Value *> replaceMethodArgs;
          if (isMetaClass) {
            CallInst *className = IRB->CreateCall(class_getName, {Class});
            CallInst *MetaClass =
                IRB->CreateCall(objc_getMetaClass, {className});
            replaceMethodArgs.push_back(MetaClass); // Class
          } else {
            replaceMethodArgs.push_back(Class); // Class
          }
          replaceMethodArgs.push_back(SEL);                         // SEL
          replaceMethodArgs.push_back(BitCastedIMP);                // imp
          replaceMethodArgs.push_back(methodStruct->getOperand(1)); // type
          IRB->CreateCall(class_replaceMethod,
                          ArrayRef<Value *>(replaceMethodArgs));
        }
      }
    }
  }
  void HandlePropertyIvar(ConstantStruct *class_ro, IRBuilder<> *IRB, Module *M,
                          Value *Class) {
    StructType *objc_property_attribute_t_type = reinterpret_cast<StructType *>(
        M->getTypeByName("struct.objc_property_attribute_t"));
    Function *class_addProperty = M->getFunction("class_addProperty");
    Function *class_addIvar = M->getFunction("class_addIvar");
    StructType *ivar_list_t_type = M->getTypeByName("struct._ivar_list_t");
    StructType *property_list_t_type = M->getTypeByName("struct._prop_list_t");
    StructType *property_t_type = M->getTypeByName("struct._prop_t");
    ConstantExpr *ivar_list = NULL;
    ConstantExpr *property_list = NULL;
    /*
      struct class_ro_t {
        uint32_t flags;
        uint32_t instanceStart;
        uint32_t instanceSize;
      #ifdef __LP64__
        uint32_t reserved;
      #endif
        const uint8_t * ivarLayout;
        const char * name;
        method_list_t * baseMethodList;
        protocol_list_t * baseProtocols;
        const ivar_list_t * ivars;
        const uint8_t * weakIvarLayout;
        property_list_t *baseProperties;
        method_list_t *baseMethods() const {
          return baseMethodList;
        }
      };*/

    // This is outrageous mess. Can we do better?
    for (unsigned i = 0; i < class_ro->getType()->getNumElements(); i++) {
      Constant *tmp = dyn_cast<Constant>(class_ro->getAggregateElement(i));
      if (tmp->isNullValue()) {
        continue;
      }
      Type *type = tmp->getType();
      if (type == ivar_list_t_type->getPointerTo()) {
        ivar_list = dyn_cast<ConstantExpr>(tmp);
      } else if (type == property_list_t_type->getPointerTo()) {
        property_list = dyn_cast<ConstantExpr>(tmp);
      }
    }
    // End Struct Loading
    // The ConstantExprs are actually BitCasts
    // We need to extract correct operands,which point to corresponding
    // GlobalVariable
    if (ivar_list != NULL) {
      GlobalVariable *GV = dyn_cast<GlobalVariable>(
          ivar_list->getOperand(0)); // Equal to casted stripPointerCasts()n
      assert(GV && "_OBJC_$_INSTANCE_VARIABLES Missing");
      assert(GV->hasInitializer() &&
             "_OBJC_$_INSTANCE_VARIABLES Missing Initializer");
      ConstantArray *ivarArray =
          dyn_cast<ConstantArray>(GV->getInitializer()->getOperand(2));
      for (unsigned i = 0; i < ivarArray->getNumOperands(); i++) {
        // struct _ivar_t
        ConstantStruct *ivar =
            dyn_cast<ConstantStruct>(ivarArray->getOperand(i));
        ConstantExpr *GEPName = dyn_cast<ConstantExpr>(ivar->getOperand(1));
        ConstantExpr *GEPType = dyn_cast<ConstantExpr>(ivar->getOperand(2));
        uint64_t alignment_junk =
            dyn_cast<ConstantInt>(ivar->getOperand(3))->getZExtValue();
        uint64_t size_junk =
            dyn_cast<ConstantInt>(ivar->getOperand(4))->getZExtValue();
        // Note alignment and size are int32 on both 32/64bit Target
        // However ObjC APIs take size_t argument, which is platform
        // dependent.WTF Apple?  We need to re-create ConstantInt with correct
        // type so we can pass verifier  Instead of doing Triple Switching
        // Again.Let's extract type from function definition
        Constant *size = ConstantInt::get(
            class_addIvar->getFunctionType()->getParamType(2), size_junk);
        Constant *alignment = ConstantInt::get(
            class_addIvar->getFunctionType()->getParamType(3), alignment_junk);
        vector<Value *> addIvar_args;
        addIvar_args.push_back(Class);
        addIvar_args.push_back(GEPName);
        addIvar_args.push_back(size);
        addIvar_args.push_back(alignment);
        addIvar_args.push_back(GEPType);
        IRB->CreateCall(class_addIvar, ArrayRef<Value *>(addIvar_args));
      }
    }
    if (property_list != NULL) {

      GlobalVariable *GV = cast<GlobalVariable>(
          property_list->getOperand(0)); // Equal to casted stripPointerCasts()
      assert(GV && "OBJC_$_PROP_LIST Missing");
      assert(GV->hasInitializer() && "OBJC_$_PROP_LIST Missing Initializer");
      ConstantArray *propArray =
          dyn_cast<ConstantArray>(GV->getInitializer()->getOperand(2));
      for (unsigned i = 0; i < propArray->getNumOperands(); i++) {
        // struct _prop_t
        ConstantStruct *prop =
            dyn_cast<ConstantStruct>(propArray->getOperand(i));
        ConstantExpr *GEPName = dyn_cast<ConstantExpr>(prop->getOperand(0));
        ConstantExpr *GEPAttri = dyn_cast<ConstantExpr>(prop->getOperand(1));
        GlobalVariable *AttrGV =
            dyn_cast<GlobalVariable>(GEPAttri->getOperand(0));
        assert(AttrGV->hasInitializer() &&
               "ObjC Property GV Don't Have Initializer");
        StringRef attrString =
            dyn_cast<ConstantDataSequential>(AttrGV->getInitializer())
                ->getAsCString();
        SmallVector<StringRef, 8> attrComponents;
        attrString.split(attrComponents, ',');
        map<string, string> propMap; // First character is key, remaining parts
                                     // are value.This is used to generate pairs
                                     // of attributes
        vector<Constant *> attrs;    // Save Each Single Attr for later use
        vector<Value *> zeroes;      // Indexes used for creating GEP
        zeroes.push_back(
            ConstantInt::get(Type::getInt32Ty(M->getContext()), 0));
        zeroes.push_back(
            ConstantInt::get(Type::getInt32Ty(M->getContext()), 0));
        for (StringRef s : attrComponents) {
          StringRef key = s.substr(0, 1);
          StringRef value = s.substr(1);
          propMap[key] = value;
          vector<Constant *> tmp;
          Constant *KeyConst =
              dyn_cast<Constant>(IRB->CreateGlobalStringPtr(key));
          Constant *ValueConst =
              dyn_cast<Constant>(IRB->CreateGlobalStringPtr(value));
          tmp.push_back(KeyConst);
          tmp.push_back(ValueConst);
          Constant *attr =
              ConstantStruct::get(property_t_type, ArrayRef<Constant *>(tmp));
          attrs.push_back(attr);
        }
        ArrayType *ATType = ArrayType::get(property_t_type, attrs.size());
        Constant *CA = ConstantArray::get(ATType, ArrayRef<Constant *>(attrs));
        AllocaInst *attrInMem = IRB->CreateAlloca(ATType);
        IRB->CreateStore(CA, attrInMem);
        // attrInMem has type [n x %struct.objc_property_attribute_t]*
        // We need to bitcast it to %struct.objc_property_attribute_t* to silent
        // GEP's type check
        Value *BitCastedFromArrayToPtr = IRB->CreateBitCast(
            attrInMem, objc_property_attribute_t_type->getPointerTo());
        Value *GEP = IRB->CreateInBoundsGEP(BitCastedFromArrayToPtr, zeroes);
        // Now GEP is done.
        // BitCast it back to our required type
        Value *GEPBitCasedToArg = IRB->CreateBitCast(
            GEP, class_addProperty->getFunctionType()->getParamType(2));
        vector<Value *> addProp_args;
        addProp_args.push_back(Class);
        addProp_args.push_back(GEPName);
        addProp_args.push_back(GEPBitCasedToArg);
        addProp_args.push_back(ConstantInt::get(
            class_addProperty->getFunctionType()->getParamType(3),
            attrs.size()));
        IRB->CreateCall(class_addProperty, ArrayRef<Value *>(addProp_args));
      }
    }
  }
};
} // namespace llvm
ModulePass *llvm::createAntiClassDumpPass() { return new AntiClassDump(); }
char AntiClassDump::ID = 0;
INITIALIZE_PASS(AntiClassDump, "acd", "Enable Anti-ClassDump.", true, true)
