/*
 *  LLVM AntiClassDump Pass
 *  https://github.com/Naville
 *  GPL V3 Licensed
 *  Note we only aim to support Darwin ObjC. GNUStep and other implementations
 *  are not considered
 *  See HikariProject's blog for details
 */
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <algorithm>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <string>
using namespace llvm;
using namespace std;
static cl::opt<bool>
    EnableAntiClassDump("enable-acd", cl::init(false), cl::NotHidden,
                        cl::desc("Enable Anti class-dump.Use with LTO"));
namespace llvm {
struct AntiClassDump : public ModulePass {
  static char ID;
  AntiClassDump() : ModulePass(ID) {}
  virtual bool doInitialization(Module &M) override {
    // Basic Defs
    Type *Int64Ty = Type::getInt64Ty(M.getContext());
    Type *Int32Ty = Type::getInt32Ty(M.getContext());
    Type *Int8PtrTy = Type::getInt8PtrTy(M.getContext());
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
    FunctionType *objc_registerClassPair_type =
        FunctionType::get(Type::getVoidTy(M.getContext()), {Int8PtrTy}, false);
    M.getOrInsertFunction("objc_registerClassPair",
                          objc_registerClassPair_type);
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
    AttributeSet SExtAttr = AttributeSet();
    SExtAttr = SExtAttr.addAttribute(M.getContext(), Attribute::SExt);
    AttributeSet ZExtAttr = AttributeSet();
    ZExtAttr = ZExtAttr.addAttribute(M.getContext(), Attribute::ZExt);
    vector<AttributeSet> c_aIArgAttributes;
    c_aIArgAttributes.push_back(AttributeSet());
    c_aIArgAttributes.push_back(AttributeSet());
    c_aIArgAttributes.push_back(AttributeSet());
    c_aIArgAttributes.push_back(ZExtAttr);
    c_aIArgAttributes.push_back(AttributeSet());
    AttributeList class_addIvar_attr =
        AttributeList::get(M.getContext(), AttributeSet(), SExtAttr,
                           ArrayRef<AttributeSet>(c_aIArgAttributes));
    AttributeList class_addProperty_attr = AttributeList::get(
        M.getContext(), AttributeSet(), SExtAttr, ArrayRef<AttributeSet>());
    vector<Type *> allocaClsTypeVector;
    allocaClsTypeVector.push_back(Int8PtrTy);
    allocaClsTypeVector.push_back(Int8PtrTy);
    if (M.getTargetTriple().compare(0, strlen("arm64-apple-ios"),
                                    "arm64-apple-ios") == 0 ||
        M.getTargetTriple().find("x86_64-apple-macosx") != string::npos) {
      // We are 64Bit Device
      allocaClsTypeVector.push_back(Int64Ty);
      M.getOrInsertFunction("class_addIvar", class_addIvar_attr,
                            Type::getInt8Ty(M.getContext()), Int8PtrTy,
                            Int8PtrTy, Int64Ty, Type::getInt8Ty(M.getContext()),
                            Int8PtrTy);
      M.getOrInsertFunction("class_addProperty", class_addProperty_attr,
                            Type::getInt8Ty(M.getContext()), Int8PtrTy,
                            Int8PtrTy,
                            objc_property_attribute_t_type->getPointerTo(),
                            Int64Ty); // objc_property_attribute_t_type is NULL?
    } else if (M.getTargetTriple().find("-apple-ios") != string::npos ||
               M.getTargetTriple().find("-apple-macosx") != string::npos) {
      // Not 64Bit.However we are still on apple platform.So We are
      // ARMV7/ARMV7S/i386
      // PowerPC is ignored, feel free to open a PR if you want to
      allocaClsTypeVector.push_back(Int32Ty);
      M.getOrInsertFunction("class_addIvar", class_addIvar_attr,
                            Type::getInt8Ty(M.getContext()), Int8PtrTy,
                            Int8PtrTy, Int32Ty, Type::getInt8Ty(M.getContext()),
                            Int8PtrTy);
      M.getOrInsertFunction(
          "class_addProperty", class_addProperty_attr,
          Type::getInt8Ty(M.getContext()), Int8PtrTy, Int8PtrTy,
          objc_property_attribute_t_type->getPointerTo(), Int32Ty);
    } else {
      errs() << M.getTargetTriple()
             << " is Not Supported For LLVM AntiClassDump\nProbably GNU Step?";
      return true;
    }
    FunctionType *allocaClsType = FunctionType::get(
        Int8PtrTy, ArrayRef<Type *>(allocaClsTypeVector), false);
    M.getOrInsertFunction("objc_allocateClassPair", allocaClsType);
    return true;
  }
  bool runOnModule(Module &M) override {
    GlobalVariable *OLCGV = M.getGlobalVariable("OBJC_LABEL_CLASS_$", true);
    // Create our own Initializer
    FunctionType *InitializerType = FunctionType::get(
        Type::getVoidTy(M.getContext()), ArrayRef<Type *>(), false);
    Function *Initializer = Function::Create(
        InitializerType, GlobalValue::LinkageTypes::PrivateLinkage, "", &M);
    BasicBlock *EntryBB = BasicBlock::Create(M.getContext(), "", Initializer);
    IRBuilder<> IRB(EntryBB);
    //
    if (OLCGV == NULL) {
      errs() << "OBJC_LABEL_CLASS_$ Not Found in IR.\nIs the target using "
                "unsupported legacy runtime?\n";
      return false;
    }
    if (!OLCGV->hasInitializer()) {
      errs() << "OBJC_LABEL_CLASS_$ Doesn't Have Initializer\n";
      return false;
    }
    ConstantArray *OBJC_LABEL_CLASS_CDS =
        dyn_cast<ConstantArray>(OLCGV->getInitializer());
    if (!OBJC_LABEL_CLASS_CDS) {
      errs() << "OBJC_LABEL_CLASS_$ Not ConstantArray.\nIs the target using "
                "unsupported legacy runtime?\n";
      return false;
    }
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
    //Sort Initialize Sequence Based On Dependency
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
      handleClass(GVMapping[className], &IRB);
    }
    // TODO:Wipe out old Structures
    // TODO:Add our initializer to llvm.global_ctors
    // Append Terminator
    IRB.CreateRetVoid();
    return true;
  } // runOnModule
  void handleClass(GlobalVariable *GV, IRBuilder<> *IRB) {

    assert(GV->hasInitializer() &&
           "ObjC Class Structure's Initializer Missing");
    ConstantStruct *CS = dyn_cast<ConstantStruct>(GV->getInitializer());
    StringRef ClassName = GV->getName();
    ClassName = ClassName.substr(strlen("OBJC_CLASS_$_"));
    StringRef SuperClassName = CS->getOperand(1)->getName();
    SuperClassName = SuperClassName.substr(strlen("OBJC_CLASS_$_"));
    errs() << "Handling Class:" << ClassName
           << " With SuperClass:" << SuperClassName << "\n";

    // We now prepare ObjC API Definitions

    Module *M = IRB->GetInsertBlock()->getModule();
    StructType *objc_property_attribute_t_type = reinterpret_cast<StructType *>(
        M->getTypeByName("struct.objc_property_attribute_t"));
    Function *objc_getClass = M->getFunction("objc_getClass");
    Function *objc_allocateClassPair = M->getFunction("objc_allocateClassPair");
    Function *objc_registerClassPair = M->getFunction("objc_registerClassPair");
    Function *class_addProperty = M->getFunction("class_addProperty");
    Function *objc_getMetaClass = M->getFunction("objc_getMetaClass");
    Function *sel_registerName = M->getFunction("sel_registerName");
    Function *class_replaceMethod = M->getFunction("class_replaceMethod");
    Function *class_addIvar = M->getFunction("class_addIvar");
    Type *Int8PtrTy = Type::getInt8PtrTy(M->getContext());
    FunctionType *IMPType =
        FunctionType::get(Int8PtrTy, {Int8PtrTy, Int8PtrTy}, true);
    PointerType *IMPPointerType = PointerType::get(IMPType, 0);
    // End of ObjC API Definitions
    // Start Allocating the class first
    Value *SuperClassNameGV = IRB->CreateGlobalStringPtr(SuperClassName);
    Value *ClassNameGV = IRB->CreateGlobalStringPtr(ClassName);
    CallInst *BaseClass = IRB->CreateCall(objc_getClass, {ClassNameGV});
    vector<Value*> allocateClsArgs;
    allocateClsArgs.push_back(BaseClass);
    allocateClsArgs.push_back(ClassNameGV);
    allocateClsArgs.push_back(ConstantInt::get(objc_allocateClassPair->getFunctionType()->getParamType(2), 0));
    CallInst *Class = IRB->CreateCall(objc_allocateClassPair,ArrayRef<Value*>(allocateClsArgs));
    //Let's extract stuffs
    // struct _class_t {
    //   struct _class_t *isa;
    //   struct _class_t * const superclass;
    //   void *cache;
    //   IMP *vtable;
    //   struct class_ro_t *ro;
    // }
    ConstantStruct *metaclass_ro=dyn_cast<ConstantStruct>(CS->getOperand(0));
    ConstantStruct *class_ro=dyn_cast<ConstantStruct>(CS->getOperand(4));
    //Now Scan For Props and Ivars in OBJC_CLASS_RO AND OBJC_METACLASS_RO
    //Note that class_ro_t's structure is different for 32 and 64bit runtime
    HandlePropertyIvar(metaclass_ro,IRB,false);
    HandlePropertyIvar(class_ro,IRB,true);
    IRB->CreateCall(objc_registerClassPair,{Class});
    //FIXME:Fix ro flags
    //Now Metadata is available in Runtime.
    //TODO:Add Methods

  }
  void HandlePropertyIvar(ConstantStruct * class_ro,IRBuilder<>* IRB,bool isClassRO){

  }
};
void addAntiClassDumpPass(legacy::PassManagerBase &PM) {
  if (EnableAntiClassDump) {
    PM.add(new AntiClassDump());
  }
}
} // namespace llvm

char AntiClassDump::ID = 0;
