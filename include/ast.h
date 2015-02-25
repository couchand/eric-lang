// ast

#ifndef _AST_H
#define _AST_H

#include <string>
#include <vector>

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Module.h"

using namespace llvm;

class ExprAST {
public:
  virtual ~ExprAST() {}
  virtual Value *Codegen() = 0;
  virtual Type *Typecheck() = 0;
};

class IntegerExprAST : public ExprAST {
  int Val;
public:
  IntegerExprAST(int val)
    : Val(val) {}
  virtual Value *Codegen();
  virtual Type *Typecheck();
};

class NumberExprAST : public ExprAST {
  double Val;
public:
  NumberExprAST(double val)
    : Val(val) {}
  virtual Value *Codegen();
  virtual Type *Typecheck();
};

class VariableExprAST : public ExprAST {
  std::string Name;
public:
  VariableExprAST(const std::string &name)
    : Name(name) {}
  virtual Value *Codegen();
  virtual Type *Typecheck();
};

class BinaryExprAST : public ExprAST {
  char Op;
  ExprAST *LHS, *RHS;
public:
  BinaryExprAST(char op, ExprAST *lhs, ExprAST *rhs)
    : Op(op), LHS(lhs), RHS(rhs) {}
  virtual Value *Codegen();
  virtual Type *Typecheck();
};

class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<ExprAST*> Args;
public:
  CallExprAST(const std::string &callee, std::vector<ExprAST*> &args)
    : Callee(callee), Args(args) {}
  virtual Value *Codegen();
  virtual Type *Typecheck();
};

class PrototypeAST {
  std::string Name;
  std::string Returns;
  std::vector<std::string> ArgTypes;
  std::vector<std::string> ArgNames;
public:
  PrototypeAST(
    const std::string &name,
    const std::string &returns,
    const std::vector<std::string> &argtypes,
    const std::vector<std::string> &argnames
  )
    : Name(name), Returns(returns), ArgTypes(argtypes), ArgNames(argnames) {}
  Function *Codegen();
  FunctionType *Typecheck();

  const std::string getName() { return Name; }
};

class FunctionAST {
  PrototypeAST *Proto;
  ExprAST *Body;
public:
  FunctionAST(PrototypeAST *proto, ExprAST *body)
    : Proto(proto), Body(body) {}
  Function *Codegen();
  FunctionType *Typecheck();
};

#endif
