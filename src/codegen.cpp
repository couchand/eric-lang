// codegen

#include <cstdio>
#include <string>
#include <map>
#include <vector>

#include "ast.h"

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
static IRBuilder<> Builder(getGlobalContext());
static std::map<std::string, Value*> NamedValues;

// debug info

static DIBuilder *DBuilder;

struct DebugInfo {

  DICompileUnit Module;
  DIFile Unit;
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

  InitializeBasicTypes(Context, DBuilder);
}

static DIType getDebugType(Type *type) {
  if (type->isIntegerTy(1)) {
    return TypeData::getType("boolean")->getDIType(&EricDebugInfo.Unit, DBuilder);
  }
  else if (type->isIntegerTy()) {
    return TypeData::getType("integer")->getDIType(&EricDebugInfo.Unit, DBuilder);
  }
  else { //if (type->isFloatingPointTy()) {
    return TypeData::getType("number")->getDIType(&EricDebugInfo.Unit, DBuilder);
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

void CreateMainFunction(std::vector<Function*> expressions) {
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
    result = Builder.CreateCall(expressions[i], std::vector<Value*>());
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
    }
  }
  else if (T->isFloatingPointTy()) {
    switch (Op) {
    default:  return ErrorV(this, "invalid binary operator");
    case '+': return Builder.CreateFAdd(L, R, "addtmp");
    case '-': return Builder.CreateFSub(L, R, "subtmp");
    case '*': return Builder.CreateFMul(L, R, "multmp");
    }
  }
  else if (T->isIntegerTy()) {
    switch (Op) {
    default:  return ErrorV(this, "invalid binary operator");
    case '+': return Builder.CreateAdd(L, R, "addtmp");
    case '-': return Builder.CreateSub(L, R, "subtmp");
    case '*': return Builder.CreateMul(L, R, "multmp");
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

      std::string message = "Casting ";
      message += source->getName();
      message += " to ";
      message += target->getName();
      fprintf(stdout, "Casting %s to %s\n", source->getName().c_str(), target->getName().c_str());

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

  return Builder.CreateCall(CalleeF, ArgsV, "calltmp");
}

Function *PrototypeAST::Codegen() {
  FunctionType *FT = (FunctionType *)Typecheck()->getLLVMType();
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

  DIDescriptor fContext(EricDebugInfo.Unit);
  DISubprogram SP = DBuilder->createFunction(
    fContext,                                   // file
    Name,                                       // name
    "",                                         // ??
    EricDebugInfo.Unit,                         // file
    Location.Line,                              // line number
    CreateFunctionType(FT),                     // function type
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

    DIScope *Scope = EricDebugInfo.LexicalBlocks.back();
    DIVariable D = DBuilder->createLocalVariable(
      dwarf::DW_TAG_arg_variable,
      *Scope,
      ArgNames[Idx],
      EricDebugInfo.Unit,
      Location.Line,
      TypeData::getType(ArgTypes[Idx])->getDIType(&EricDebugInfo.Unit, DBuilder),
      Idx
    );
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

  Builder.CreateRet(RetVal);

  EricDebugInfo.LexicalBlocks.pop_back();

  verifyFunction(*TheFunction);

  return TheFunction;
}
