// codegen

#include <cstdio>
#include <map>

#include "ast.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/IR/TypeBuilder.h"
// llvm 3.5 headers
//#include "llvm/IR/Verifier.h"
//#include "llvm/Support/TypeBuilder.h"

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

static Module *TheModule;
static IRBuilder<> Builder(getGlobalContext());
static std::map<std::string, Value*> NamedValues;

void InitializeCodegen() {
  LLVMContext &Context = getGlobalContext();
  TheModule = new Module("eric repl", Context);
}

void CreateMainFunction(std::vector<Function*> expressions) {
  FunctionType *mainType = TypeBuilder<types::i<64>(), true>::get(getGlobalContext());
  Function *main = Function::Create(mainType, Function::ExternalLinkage, "main", TheModule);

  BasicBlock *BB = BasicBlock::Create(getGlobalContext(), "entry", main);
  Builder.SetInsertPoint(BB);

  Value *result;

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

  verifyFunction(*main);
}

void DumpAllCode() {
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

  Type *T = Typecheck();
  if (!T) return 0;

  Type *LT = LHS->Typecheck();

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
  if (Callee == "integer") {
    if (Args.size() != 1) {
      return ErrorV(this, "Integer cast expects a single argument");
    }

    Type *T = Args[0]->Typecheck();
    if (!T) return 0;

    if (T->isIntegerTy()) {
      // assume one integer type (TODO: not)
      return Args[0]->Codegen();
    }
    else if (T->isFloatingPointTy()) {
      Value *Source = Args[0]->Codegen();
      if (!Source) return 0;

      return Builder.CreateFPToSI(Source, Type::getInt64Ty(getGlobalContext()), "casttmp");
    }
    else {
      return ErrorV(this, "Unable to cast type to integer");
    }
  }

  if (Callee == "number") {
    if (Args.size() != 1) {
      return ErrorV(this, "Number cast expects a single argument");
    }

    Type *T = Args[0]->Typecheck();
    if (!T) return 0;

    if (T->isFloatingPointTy()) {
      // assume one float type (TODO: not)
      return Args[0]->Codegen();
    }
    else if (T->isIntegerTy()) {
      Value *Source = Args[0]->Codegen();
      if (!Source) return 0;

      return Builder.CreateSIToFP(Source, Type::getDoubleTy(getGlobalContext()), "casttmp");
    }
    else {
      return ErrorV(this, "Unable to cast type to integer");
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
  FunctionType *FT = Typecheck();
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

  unsigned Idx = 0;
  for (Function::arg_iterator AI = F->arg_begin(); Idx != ArgTypes.size(); ++AI, ++Idx) {
    AI->setName(ArgNames[Idx]);

    NamedValues[ArgNames[Idx]] = AI;
  }

  return F;
}

Function *FunctionAST::Codegen() {
  NamedValues.clear();

  Function *TheFunction = Proto->Codegen();
  if (!TheFunction) return 0;

  BasicBlock *BB = BasicBlock::Create(getGlobalContext(), "entry", TheFunction);
  Builder.SetInsertPoint(BB);

  Value *RetVal = Body->Codegen();
  if (!RetVal) {
    TheFunction->eraseFromParent();
    return 0;
  }

  Builder.CreateRet(RetVal);
  verifyFunction(*TheFunction);

  return TheFunction;
}
