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
#include "llvm/ADT/Triple.h"
#include <algorithm>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <string>
#include <cassert>
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
    Triple tri(M.getTargetTriple());
    if(tri.getVendor ()!=Triple::VendorType::Apple){
      //We only support AAPL's ObjC Implementation ATM
        errs() << M.getTargetTriple()
               << " is Not Supported For LLVM AntiClassDump\nProbably GNU Step?\n";
      return false;
    }
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
    if (tri.isArch64Bit ()) {
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
    } else{
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
    assert(OLCGV != NULL && "OBJC_LABEL_CLASS_$ Missing.");
    assert(OLCGV->hasInitializer() && "OBJC_LABEL_CLASS_$ Doesn't Have Initializer.");
    ConstantArray *OBJC_LABEL_CLASS_CDS =
        dyn_cast<ConstantArray>(OLCGV->getInitializer());

    assert(OBJC_LABEL_CLASS_CDS && "OBJC_LABEL_CLASS_$ Not ConstantArray.Is the target using unsupported legacy runtime?");
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
    Function *objc_getClass = M->getFunction("objc_getClass");
    Function *objc_allocateClassPair = M->getFunction("objc_allocateClassPair");
    Function *objc_registerClassPair = M->getFunction("objc_registerClassPair");
    Function *objc_getMetaClass = M->getFunction("objc_getMetaClass");
    Function *sel_registerName = M->getFunction("sel_registerName");
    Function *class_replaceMethod = M->getFunction("class_replaceMethod");
    Type *Int8PtrTy = Type::getInt8PtrTy(M->getContext());
    FunctionType *IMPType =
        FunctionType::get(Int8PtrTy, {Int8PtrTy, Int8PtrTy}, true);
    PointerType *IMPPointerType = PointerType::get(IMPType, 0);
    // End of ObjC API Definitions
    // Start Allocating the class first
    Value *SuperClassNameGV = IRB->CreateGlobalStringPtr(SuperClassName);
    Value *ClassNameGV = IRB->CreateGlobalStringPtr(ClassName);
    CallInst *BaseClass = IRB->CreateCall(objc_getClass, {ClassNameGV});
    vector<Value *> allocateClsArgs;
    allocateClsArgs.push_back(BaseClass);
    allocateClsArgs.push_back(ClassNameGV);
    allocateClsArgs.push_back(ConstantInt::get(
        objc_allocateClassPair->getFunctionType()->getParamType(2), 0));
    CallInst *Class = IRB->CreateCall(objc_allocateClassPair,
                                      ArrayRef<Value *>(allocateClsArgs));
    // Let's extract stuffs
    // struct _class_t {
    //   struct _class_t *isa;
    //   struct _class_t * const superclass;
    //   void *cache;
    //   IMP *vtable;
    //   struct class_ro_t *ro;
    // }
    GlobalVariable *metaclass_ro = dyn_cast<GlobalVariable>(CS->getOperand(0));
    GlobalVariable *class_ro = dyn_cast<GlobalVariable>(CS->getOperand(4));
    // Now Scan For Props and Ivars in OBJC_CLASS_RO AND OBJC_METACLASS_RO
    // Note that class_ro_t's structure is different for 32 and 64bit runtime
    if (ConstantStruct *CS =
            dyn_cast<ConstantStruct>(class_ro->getInitializer())) {
      HandlePropertyIvar(CS, IRB, M,Class);
    }
    IRB->CreateCall(objc_registerClassPair, {Class});
    // FIXME:Fix ro flags
    // Now Metadata is available in Runtime.
    // TODO:Add Methods
  }
  void HandlePropertyIvar(ConstantStruct *class_ro, IRBuilder<> *IRB,
                          Module *M,Value* Class) {
    StructType *objc_property_attribute_t_type = reinterpret_cast<StructType *>(
        M->getTypeByName("struct.objc_property_attribute_t"));
    Function *class_addProperty = M->getFunction("class_addProperty");
    Function *class_addIvar = M->getFunction("class_addIvar");
    StructType *ivar_list_t_type = M->getTypeByName("struct._ivar_list_t");
    StructType *property_list_t_type = M->getTypeByName("struct._prop_list_t");
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
      if(tmp->isNullValue ()){
        continue;
      }
      Type* type=tmp->getType();
      if(type==ivar_list_t_type->getPointerTo()){
        ivar_list=dyn_cast<ConstantExpr>(tmp);
      }
      else if(type==property_list_t_type->getPointerTo()){
        property_list=dyn_cast<ConstantExpr>(tmp);
      }
    }
    // End Struct Loading
    //The ConstantExprs are actually BitCasts
    //We need to extract correct operands,which point to corresponding GlobalVariable
    if(ivar_list!=NULL){
      GlobalVariable* GV=dyn_cast<GlobalVariable>(ivar_list->getOperand(0));
      assert(GV&& "_OBJC_$_INSTANCE_VARIABLES Missing");
      assert(GV->hasInitializer() && "_OBJC_$_INSTANCE_VARIABLES Missing Initializer");
      ConstantArray *ivarArray=dyn_cast<ConstantArray>(GV->getInitializer()->getOperand(2));
      for(unsigned i=0;i<ivarArray->getNumOperands();i++){
        //struct _ivar_t
        ConstantStruct* ivar=dyn_cast<ConstantStruct>(ivarArray->getOperand(i));
        ConstantExpr* GEPName=dyn_cast<ConstantExpr>(ivar->getOperand(1));
        ConstantExpr* GEPType=dyn_cast<ConstantExpr>(ivar->getOperand(2));
        uint64_t alignment_junk=dyn_cast<ConstantInt>(ivar->getOperand(3))->getZExtValue () ;
        uint64_t size_junk=dyn_cast<ConstantInt>(ivar->getOperand(4))->getZExtValue () ;
        //Note alignment and size are int32 on both 32/64bit Target
        //However ObjC APIs take size_t argument, which is platform dependent.WTF Apple?
        //We need to re-create ConstantInt with correct type so we can pass verifier
        //Instead of doing Triple Switching Again.Let's extract type from function definition
        Constant* size=ConstantInt::get(class_addIvar->getFunctionType()->getParamType(2),size_junk);
        Constant* alignment=ConstantInt::get(class_addIvar->getFunctionType()->getParamType(3),alignment_junk);
        vector<Value*> addIvar_args;
        addIvar_args.push_back(Class);
        addIvar_args.push_back(GEPName);
        addIvar_args.push_back(size);
        addIvar_args.push_back(alignment);
        addIvar_args.push_back(GEPType);
        IRB->CreateCall(class_addIvar,ArrayRef<Value*>(addIvar_args));
      }
    }
    if(property_list!=NULL){
      GlobalVariable* GV=dyn_cast<GlobalVariable>(property_list->getOperand(0));
      assert(GV&& "OBJC_$_PROP_LIST Missing");
      assert(GV->hasInitializer() && "OBJC_$_PROP_LIST Missing Initializer");
      ConstantArray *propArray=dyn_cast<ConstantArray>(GV->getInitializer()->getOperand(2));
      for(unsigned i=0;i<propArray->getNumOperands();i++){
        //struct _prop_t
        ConstantStruct* prop=dyn_cast<ConstantStruct>(propArray->getOperand(i));
        ConstantExpr* GEPName=dyn_cast<ConstantExpr>(prop->getOperand(0));
        ConstantExpr* GEPAttri=dyn_cast<ConstantExpr>(prop->getOperand(1));
        GlobalVariable *AttrGV=dyn_cast<GlobalVariable>(GEPAttri->getOperand(0));
        assert(AttrGV->hasInitializer() && "ObjC Property GV Don't Have Initializer");
        StringRef attrString=dyn_cast<ConstantDataSequential>(AttrGV->getInitializer())->getAsCString();
        errs()<<attrString<<"\n";
        vector<Value*> addProp_args;
        addProp_args.push_back(Class);
        addProp_args.push_back(GEPName);
        //TODO: Create GEPs to load attribute
        //TODO: ADD ATTR COUNT
        //IRB->CreateCall(class_addProperty,ArrayRef<Value*>(addProp_args));
      }
    }
  }
};
void addAntiClassDumpPass(legacy::PassManagerBase &PM) {
  if (EnableAntiClassDump) {
    PM.add(new AntiClassDump());
  }
}
} // namespace llvm

char AntiClassDump::ID = 0;
