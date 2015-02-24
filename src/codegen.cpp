// codegen

#include <cstdio>
#include <map>

#include "ast.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Analysis/Verifier.h"

Value *ErrorV(const char *message) {
  fprintf(stderr, "Error while compiling to IR: %s\n", message);
  return 0;
}

Function *ErrorF2(const char *message) {
  ErrorV(message);
  return 0;
}

static Module *TheModule;
static IRBuilder<> Builder(getGlobalContext());
static std::map<std::string, Value*> NamedValues;

void InitializeCodegen() {
  LLVMContext &Context = getGlobalContext();
  TheModule = new Module("eric repl", Context);
}

void DumpAllCode() {
  TheModule->dump();
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
    return ErrorV(message.c_str());
  }

  return V;
}

Value *BinaryExprAST::Codegen() {
  Value *L = LHS->Codegen();
  Value *R = RHS->Codegen();
  if (!L || !R) return 0;

  switch (Op) {
  default:
    return ErrorV("invalid binary operator");
  case '+': return Builder.CreateFAdd(L, R, "addtmp");
  case '-': return Builder.CreateFSub(L, R, "subtmp");
  case '*': return Builder.CreateFMul(L, R, "multmp");
  case '<':
    L = Builder.CreateFCmpULT(L, R, "cmptmp");
    return Builder.CreateUIToFP(L, Type::getDoubleTy(getGlobalContext()), "booltmp");
  }
}

Value *CallExprAST::Codegen() {
  Function *CalleeF = TheModule->getFunction(Callee);
  if (!CalleeF) {
    std::string message = "Unknown function reference: ";
    message += Callee;
    return ErrorV(message.c_str());
  }

  if (CalleeF->arg_size() != Args.size())
    return ErrorV("Wrong number of arguments to function");

  std::vector<Value*> ArgsV;
  for (unsigned i = 0, e = Args.size(); i < e; i++) {
    ArgsV.push_back(Args[i]->Codegen());
    if (!ArgsV.back()) return 0;
  }

  return Builder.CreateCall(CalleeF, ArgsV, "calltmp");
}

Function *PrototypeAST::Codegen() {
  std::vector<Type*> Doubles(ArgTypes.size(), Type::getDoubleTy(getGlobalContext()));
  FunctionType *FT = FunctionType::get(Type::getDoubleTy(getGlobalContext()), Doubles, false);
  Function *F = Function::Create(FT, Function::ExternalLinkage, Name, TheModule);

  if (F->getName() != Name) {
    F->eraseFromParent();
    F = TheModule->getFunction(Name);

    if (!F->empty()) {
      std::string message = "redefinition of function: ";
      message += Name;
      return ErrorF2(message.c_str());
    }

    if (F->arg_size() != ArgTypes.size()) {
      std::string message = "implementation of function has wrong arguments: ";
      message += Name;
      return ErrorF2(message.c_str());
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
