// codegen

#include <cstdio>
#include <string>
#include <map>
#include <vector>

#include "ast.h"
#include "context.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DIBuilder.h"

void CompilerError(SourceLocation loc, const char *message) {
  fprintf(stderr, "Error while compiling at line %i, column %i: %s\n", loc.Line, loc.Column, message);
}

Value *ErrorV(ExprAST *e, const char *message) {
  CompilerError(e->getLocation(), message);
  return 0;
}

Function *ErrorF2(SourceLocation loc, const char *message) {
  CompilerError(loc, message);
  return 0;
}

// GLOBALS

static Module *TheModule;
static DataLayout *DL;
static IRBuilder<> Builder(getGlobalContext());
static std::map<std::string, Value*> NamedValues;

// debug info

static DIBuilder *DBuilder;

struct DebugInfo {

  DICompileUnit Module;
  DIFile Unit;
  DebugContext *DebugContext;
  std::map<std::string, DIType> Types;

  std::vector<DIScope *> LexicalBlocks;
  std::map<const PrototypeAST *, DIScope> FnScopeMap;

  void clearLocation();
  void emitLocation(ExprAST *AST);
  void emitLocation(int line, int column);

} EricDebugInfo;

void DebugInfo::clearLocation() {
  Builder.SetCurrentDebugLocation(DebugLoc());
}

void DebugInfo::emitLocation(ExprAST *AST) {
  SourceLocation loc = AST->getLocation();

  emitLocation(loc.Line, loc.Column);
}

void DebugInfo::emitLocation(int line, int column) {
  DIScope *Scope;
  if (LexicalBlocks.empty())
    Scope = &Module;
  else
    Scope = LexicalBlocks.back();

  Builder.SetCurrentDebugLocation(
    DebugLoc::get(line, column, DIScope(*Scope))
  );
}

// functions

void InitializeDataLayout(Module *m) {
  DL = new DataLayout(m);
}

void InitializeCodegen(const char *filename) {
  LLVMContext &Context = getGlobalContext();
  TheModule = new Module("eric repl", Context);

  TheModule->addModuleFlag(Module::Warning, "Debug Info Version", DEBUG_METADATA_VERSION);

  DBuilder = new DIBuilder(*TheModule);

  EricDebugInfo.Module = DBuilder->createCompileUnit(
    dwarf::DW_LANG_C, // lang
    filename,         // file
    ".",              // dir
    "Eric Compiler",  // producer
    0,                // optimized?
    "",               // flags
    0                 // version
  );

  EricDebugInfo.Unit = DBuilder->createFile(EricDebugInfo.Module.getFilename(), EricDebugInfo.Module.getDirectory());

  EricDebugInfo.DebugContext = new DebugContext(&EricDebugInfo.Unit, DBuilder, new DataLayout(TheModule));

  InitializeDataLayout(TheModule);
  InitializeBasicTypes(Context, DBuilder);
}

static DIType getDebugType(Type *type) {
  if (type->isIntegerTy(1)) {
    return TypeData::getType("boolean")->getDIType(EricDebugInfo.DebugContext);
  }
  else if (type->isIntegerTy()) {
    return TypeData::getType("integer")->getDIType(EricDebugInfo.DebugContext);
  }
  else { //if (type->isFloatingPointTy()) {
    return TypeData::getType("number")->getDIType(EricDebugInfo.DebugContext);
  }
}

static DICompositeType CreateFunctionType(FunctionType *type) {
  SmallVector<Value *, 8> paramTypes;

  Type *returns = type->getReturnType();
  paramTypes.push_back(getDebugType(returns));

  for (unsigned i = 0, e = type->getNumParams(); i < e; i++) {
    paramTypes.push_back(getDebugType(type->getParamType(i)));
  }

  DIArray paramTypeArray = DBuilder->getOrCreateArray(paramTypes);
  return DBuilder->createSubroutineType(EricDebugInfo.Unit, paramTypeArray);
}

void CreateMainFunction(std::vector<ExprAST*> expressions) {
  FunctionType *mainType = TypeBuilder<types::i<64>(), true>::get(getGlobalContext());
  Function *main = Function::Create(mainType, Function::ExternalLinkage, "main", TheModule);

  DIDescriptor fContext(EricDebugInfo.Unit);
  DISubprogram SP = DBuilder->createFunction(
    fContext,                                   // file
    "main",                                     // name
    "",                                         // ??
    EricDebugInfo.Unit,                         // file
    0,                                          // line number
    CreateFunctionType(mainType),               // function type
    false,                                      // internal linkage
    true,                                       // definition
    0,                                          // ??
    DIDescriptor::FlagPrototyped,               // flags
    false,                                      // ??
    main                                        // the function
  );

  EricDebugInfo.LexicalBlocks.push_back(&SP);
  EricDebugInfo.clearLocation();

  BasicBlock *BB = BasicBlock::Create(getGlobalContext(), "entry", main);
  Builder.SetInsertPoint(BB);

  Value *result;

  EricDebugInfo.emitLocation(0, 0);

  for (unsigned i = 0, e = expressions.size(); i < e; i++) {
    EricDebugInfo.emitLocation(expressions[i]);
    result = expressions[i]->Codegen();
  }

  if (!result || !result->getType()->isIntegerTy(64)) {
    Value *zero = ConstantInt::get(TypeBuilder<types::i<64>, true>::get(getGlobalContext()), 0);
    Builder.CreateRet(zero);
  }
  else {
    Builder.CreateRet(result);
  }

  EricDebugInfo.LexicalBlocks.pop_back();

  verifyFunction(*main);
}

void DumpAllCode() {
  // complete debug operations
  DBuilder->finalize();

  // dump code
  TheModule->dump();
}

Value *BooleanExprAST::Codegen() {
  return ConstantInt::get(TypeBuilder<types::i<1>, true>::get(getGlobalContext()), Val);
}

Value *IntegerExprAST::Codegen() {
  return ConstantInt::get(TypeBuilder<types::i<64>, true>::get(getGlobalContext()), Val);
}

Value *NumberExprAST::Codegen() {
  return ConstantFP::get(getGlobalContext(), APFloat(Val));
}

Value *VariableExprAST::Codegen() {
  Value *V = NamedValues[Name];

  if (!V) {
    std::string message = "Unknown variable name: '";
    message += Name;
    message += "'";
    return ErrorV(this, message.c_str());
  }

  return V;
}

Value *BinaryExprAST::Codegen() {
  Value *L = LHS->Codegen();
  Value *R = RHS->Codegen();
  if (!L || !R) return 0;

  Type *T = Typecheck()->getLLVMType();
  if (!T) return 0;

  Type *LT = LHS->Typecheck()->getLLVMType();

  EricDebugInfo.emitLocation(this);
  if (T->isIntegerTy(1)) {
    switch (Op) {
    default: return ErrorV(this, "invalid binary operator");
    case '<':
      if (LT->isFloatingPointTy())
        return Builder.CreateFCmpULT(L, R, "cmptmp");
      if (LT->isIntegerTy())
        return Builder.CreateICmpSLT(L, R, "cmptmp");
    case '=':
      if (LT->isFloatingPointTy())
        return Builder.CreateFCmpUEQ(L, R, "cmptmp");
      if (LT->isIntegerTy())
        return Builder.CreateICmpEQ(L, R, "cmptmp");
    case '&':
      if (LT->isIntegerTy(1))
        return Builder.CreateAnd(L, R, "cmptmp");
    case '|':
      if (LT->isIntegerTy(1))
        return Builder.CreateOr(L, R, "cmptmp");
    }
  }
  else if (T->isFloatingPointTy()) {
    switch (Op) {
    default:  return ErrorV(this, "invalid binary operator");
    case '+': return Builder.CreateFAdd(L, R, "addtmp");
    case '-': return Builder.CreateFSub(L, R, "subtmp");
    case '*': return Builder.CreateFMul(L, R, "multmp");
    case '/': return Builder.CreateFDiv(L, R, "divtmp");
    case '%': return Builder.CreateFRem(L, R, "remtmp");
    }
  }
  else if (T->isIntegerTy()) {
    switch (Op) {
    default:  return ErrorV(this, "invalid binary operator");
    case '+': return Builder.CreateAdd(L, R, "addtmp");
    case '-': return Builder.CreateSub(L, R, "subtmp");
    case '*': return Builder.CreateMul(L, R, "multmp");
    case '/': return Builder.CreateSDiv(L, R, "divtmp");
    case '%': return Builder.CreateSRem(L, R, "remtmp");
    }
  }
  return ErrorV(this, "invalid types in binary operator");
}

Value *CallExprAST::Codegen() {
  if (Callee == "integer" || Callee == "number" || Callee == "boolean") {
    if (Args.size() != 1) {
      return ErrorV(this, "Cast expects a single argument");
    }

    TypeData *target = TypeData::getType(Callee);

    TypeData *source = Args[0]->Typecheck();
    if (!source) return 0;

    if (source == target) {
      // same type, no cast needed
      return Args[0]->Codegen();
    }
    else if (source->canConvertTo(target)) {
      Value *Source = Args[0]->Codegen();
      if (!Source) return 0;

      return source->convertTo(Builder, target, Source);
    }
    else {
      std::string message = "Unable to cast type to ";
      message += Callee;
      return ErrorV(this, message.c_str());
    }
  }

  Function *CalleeF = TheModule->getFunction(Callee);
  if (!CalleeF) {
    std::string message = "Unknown function reference: ";
    message += Callee;
    return ErrorV(this, message.c_str());
  }

  if (CalleeF->arg_size() != Args.size())
    return ErrorV(this, "Wrong number of arguments to function");

  std::vector<Value*> ArgsV;
  for (unsigned i = 0, e = Args.size(); i < e; i++) {
    ArgsV.push_back(Args[i]->Codegen());
    if (!ArgsV.back()) return 0;
  }

  EricDebugInfo.emitLocation(this);

  FunctionTypeData* fnType = FunctionTypeData::getFunctionType(Callee);

  std::string retType = fnType->getReturnType()->getName();
  //fprintf(stdout, "func call %s returns %s\n", Callee.c_str(), retType.c_str());

  //for (unsigned i = 0, e = fnType->getNumParameters(); i < e; i++) {
  //  fprintf(stdout, "  p %i type %s\n", i, fnType->getParameterType(i)->getName().c_str());
  //}

  if (retType == "void") {
    return Builder.CreateCall(CalleeF, ArgsV);
  }
  else {
    return Builder.CreateCall(CalleeF, ArgsV, "calltmp");
  }
}

Value *ArrayLiteralExprAST::Codegen() {
  TypeData *t = Typecheck();
  if (!t) return 0;

  //fprintf(stdout, "genning %s\n", t->getName().c_str());

  if (!t->isArrayType())
    return ErrorV(this, "expected array type");

  ArrayTypeData *myType = (ArrayTypeData *)t;
  if (!myType) return 0;

  Type *llvmType = myType->getLLVMType();
  if (!llvmType) return 0;

  if (myType->isEmptyArray()) {
    return ErrorV(this, "empty array never coalesced");
  }

  //fprintf(stdout, "genning %s\n", myType->getName().c_str());

  TypeData *memberType = myType->getMemberType();
  if (!memberType) {
    return ErrorV(this, "array type has no member type");
  }

  //fprintf(stdout, "genning %s\n", memberType->getName().c_str());

  Type *elementType = memberType->getLLVMType();
  if (!elementType) return 0;

  //fprintf(stdout, "genning\n");

  Type *integerType = TypeData::getType("integer")->getLLVMType();
  Type *thirtyTwoBitInteger = TypeBuilder<types::i<32>, true>::get(getGlobalContext());

  //fprintf(stdout, "genning\n");

  uint64_t size = DL->getTypeStoreSize(elementType);
  uint64_t count = Elements.size();
  uint64_t overhead = DL->getTypeStoreSize(integerType);

  Value *space = ConstantInt::get(integerType, size * count + overhead);

  Function *malloc = TheModule->getFunction("malloc");
  if (!malloc) {
    return ErrorV(this, "no malloc found");
  }

  // malloc the space for the array
  Value *mem = Builder.CreateCall(malloc, space, "malloctmp");

  // bitcast to the proper pointer type
  Value *bc = Builder.CreateBitCast(mem, llvmType, "arraytmp");

  // store the count
  Value *countPtr = Builder.CreateConstGEP2_32(bc, 0, 0, "arraycounttmp");
  Builder.CreateStore(ConstantInt::get(integerType, count), countPtr);

  // insert the values
  for (unsigned i = 0, e = Elements.size(); i < e; i++) {
    Value *el = Elements[i]->Codegen();
    if (!el) return 0;

    SmallVector<Value *, 8> idxs;
    idxs.push_back(ConstantInt::get(integerType, 0));
    idxs.push_back(ConstantInt::get(thirtyTwoBitInteger, 1));
    idxs.push_back(ConstantInt::get(integerType, i));

    Value *elPtr = Builder.CreateGEP(bc, idxs, "arrayindextmp");
    Builder.CreateStore(el, elPtr);
  }

  return bc;
}

Value *ArrayReferenceExprAST::Codegen() {
  TypeData *myType = Typecheck();

  Value *array = Source->Codegen();
  if (!array) return 0;

  Value *index = Index->Codegen();
  if (!index) return 0;

  Value *countPtr = Builder.CreateConstGEP2_32(array, 0, 0, "arraycountptrtmp");
  Value *count = Builder.CreateLoad(countPtr, "arraycounttmp");
  Value *legal = Builder.CreateICmpSLT(index, count, "legaltmp");

  // TODO: only continue if legal

  Type *integerType = TypeData::getType("integer")->getLLVMType();
  Type *thirtyTwoBitInteger = TypeBuilder<types::i<32>, true>::get(getGlobalContext());

  SmallVector<Value *, 8> idxs;
  idxs.push_back(ConstantInt::get(integerType, 0));
  idxs.push_back(ConstantInt::get(thirtyTwoBitInteger, 1));
  idxs.push_back(index);

  Value *refPtr = Builder.CreateGEP(array, idxs, "arrayindexptrtmp");
  return Builder.CreateLoad(refPtr, "arrayindextmp");
}

Value *ValueLiteralAST::Codegen() {
  TypeData *myType = Typecheck();
  if (!myType) return 0;

  Type *myT = myType->getLLVMType();
  if (!myT) return 0;

  Value *structValue = UndefValue::get(myT);

  EricDebugInfo.emitLocation(this);

  for (unsigned i = 0, e = Fields.size(); i < e; i++) {
    EricDebugInfo.emitLocation(Fields[i]);

    Value *fieldValue = Fields[i]->Codegen();
    if (!fieldValue) return 0;

    EricDebugInfo.emitLocation(this);
    structValue = Builder.CreateInsertValue(structValue, fieldValue, i);
  }

  return structValue;
}

Value *ValueReferenceAST::Codegen() {
  TypeData *td = Source->Typecheck();
  if (!td) return 0;

  if (!td->isStructType()) {
    std::string message = "Expecting value to be a structure, is ";
    message += td->getName();
    return ErrorV(this, message.c_str());
  }

  StructTypeData *st = (StructTypeData *)td;

  int idx = st->getFieldIndex(FieldReference);
  if (-1 == idx) {
    std::string message = "Value has no field named ";
    message += FieldReference;
    return ErrorV(this, message.c_str());
  }

  Value *source = Source->Codegen();
  if (!source) return 0;

  return Builder.CreateExtractValue(source, idx);
}

Value *BlockExprAST::Codegen() {
  Value * v;

  for (unsigned i = 0, e = Statements.size(); i < e; i++) {
    EricDebugInfo.emitLocation(Statements[i]);
    v = Statements[i]->Codegen();
  }

  return v;
}

Value *ConditionalExprAST::Codegen() {
  // blocks
  Function *parentFunction = Builder.GetInsertBlock()->getParent();

  BasicBlock *consequentBlock = BasicBlock::Create(getGlobalContext(), "then", parentFunction);
  BasicBlock *alternateBlock = BasicBlock::Create(getGlobalContext(), "else");
  BasicBlock *mergeBlock = BasicBlock::Create(getGlobalContext(), "ifmerge");

  // emit condition
  Value *conditionValue = Condition->Codegen();
  if (!conditionValue) return 0;

  EricDebugInfo.emitLocation(this);
  Builder.CreateCondBr(conditionValue, consequentBlock, alternateBlock);

  // emit consequent
  Builder.SetInsertPoint(consequentBlock);

  EricDebugInfo.emitLocation(Consequent);
  Value *consequentResult = Consequent->Codegen();
  if (!consequentResult) return 0;

  Builder.CreateBr(mergeBlock);
  consequentBlock = Builder.GetInsertBlock(); // in case of nested blocks

  // emit alternate
  parentFunction->getBasicBlockList().push_back(alternateBlock);
  Builder.SetInsertPoint(alternateBlock);

  EricDebugInfo.emitLocation(Alternate);
  Value *alternateResult = Alternate->Codegen();
  if (!alternateResult) return 0;

  Builder.CreateBr(mergeBlock);
  alternateBlock = Builder.GetInsertBlock(); // in case of nested blocks

  // emit merge
  parentFunction->getBasicBlockList().push_back(mergeBlock);
  Builder.SetInsertPoint(mergeBlock);

  TypeData *mergedType = Typecheck();

  EricDebugInfo.emitLocation(this);

  if (mergedType->getName() == "void") {
    return UndefValue::get(TypeBuilder<types::i<1>, true>::get(getGlobalContext()));
  }
  else
  {
    PHINode *phi = Builder.CreatePHI(mergedType->getLLVMType(), 2, "iftmp");
    phi->addIncoming(consequentResult, consequentBlock);
    phi->addIncoming(alternateResult, alternateBlock);
    return phi;
  }
}

TypeData *ValueTypeAST::MakeType() {
  std::vector<TypeData *> ts;
  for (unsigned i = 0, e = ElementNames.size(); i < e; i++) {
    TypeData *t = TypeData::getType(ElementTypes[i]);
    if (!t) return 0;

    ts.push_back(t);
  }

  TypeData *typeData = new StructTypeData(Name, ts, ElementNames);

  TypeData::registerType(typeData);

  return typeData;
}

Function *PrototypeAST::Codegen() {
  TypeData *t = Typecheck();
  if (!t) return 0;

  FunctionType *FT = (FunctionType *)t->getLLVMType();
  Function *F = Function::Create(FT, Function::ExternalLinkage, Name, TheModule);

  if (F->getName() != Name) {
    F->eraseFromParent();
    F = TheModule->getFunction(Name);

    if (!F->empty()) {
      std::string message = "redefinition of function: ";
      message += Name;
      return ErrorF2(Location, message.c_str());
    }

    if (F->arg_size() != ArgTypes.size()) {
      std::string message = "implementation of function has wrong arguments: ";
      message += Name;
      return ErrorF2(Location, message.c_str());
    }
  }

  DICompositeType debugType = (DICompositeType)t->getDIType(EricDebugInfo.DebugContext);

  DIDescriptor fContext(EricDebugInfo.Unit);
  DISubprogram SP = DBuilder->createFunction(
    fContext,                                   // file
    Name,                                       // name
    "",                                         // ??
    EricDebugInfo.Unit,                         // file
    Location.Line,                              // line number
    debugType,                                  // function type
    false,                                      // internal linkage
    true,                                       // definition
    0,                                          // ??
    DIDescriptor::FlagPrototyped,               // flags
    false,                                      // ??
    F                                           // the function
  );

  EricDebugInfo.FnScopeMap[this] = SP;

  return F;
}

void PrototypeAST::UpdateArguments(Function *F) {
  unsigned Idx = 0;
  for (Function::arg_iterator AI = F->arg_begin(); Idx != ArgTypes.size(); ++AI, ++Idx) {
    AI->setName(ArgNames[Idx]);

    NamedValues[ArgNames[Idx]] = AI;

    TypeData *argType = TypeData::getType(ArgTypes[Idx]);
    if (!argType) fprintf(stderr, "Error retrieving argument type.\n");

    DIScope *Scope = EricDebugInfo.LexicalBlocks.back();
    DIVariable D = DBuilder->createLocalVariable(
      dwarf::DW_TAG_arg_variable,
      *Scope,
      ArgNames[Idx],
      EricDebugInfo.Unit,
      Location.Line,
      argType->getDIType(EricDebugInfo.DebugContext),
      Idx
    );

    llvm::Instruction *Call = DBuilder->insertDeclare(AI, D, Builder.GetInsertBlock());
    Call->setDebugLoc(DebugLoc::get(Location.Line, Location.Column, *Scope));
  }
}

Function *FunctionAST::Codegen() {
  NamedValues.clear();

  Function *TheFunction = Proto->Codegen();
  if (!TheFunction) return 0;

  EricDebugInfo.LexicalBlocks.push_back(&EricDebugInfo.FnScopeMap[Proto]);
  EricDebugInfo.clearLocation();

  BasicBlock *BB = BasicBlock::Create(getGlobalContext(), "entry", TheFunction);
  Builder.SetInsertPoint(BB);

  Proto->UpdateArguments(TheFunction);

  EricDebugInfo.emitLocation(Body);

  Value *RetVal = Body->Codegen();
  if (!RetVal) {
    TheFunction->eraseFromParent();
    EricDebugInfo.LexicalBlocks.pop_back();
    return 0;
  }

  if (Proto->Typecheck()->getReturnType()->getName() == "void") {
    Builder.CreateRetVoid();
  }
  else {
    Builder.CreateRet(RetVal);
  }

  EricDebugInfo.LexicalBlocks.pop_back();

  verifyFunction(*TheFunction);

  return TheFunction;
}
