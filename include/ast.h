// ast

#ifndef _AST_H
#define _AST_H

#include <string>
#include <vector>

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Module.h"

#include "lexer.h"

using namespace llvm;

class ExprAST {
  SourceLocation Location;

public:
  virtual ~ExprAST() {}
  virtual Value *Codegen() = 0;
  virtual Type *Typecheck() = 0;

  ExprAST(SourceLocation loc)
    : Location(loc) {}

  SourceLocation getLocation() const { return Location; }
};

class BooleanExprAST : public ExprAST {
  bool Val;
public:
  BooleanExprAST(SourceLocation loc, bool val)
    : ExprAST(loc), Val(val) {}
  virtual Value *Codegen();
  virtual Type *Typecheck();
};

class IntegerExprAST : public ExprAST {
  int Val;
public:
  IntegerExprAST(SourceLocation loc, int val)
    : ExprAST(loc), Val(val) {}
  virtual Value *Codegen();
  virtual Type *Typecheck();
};

class NumberExprAST : public ExprAST {
  double Val;
public:
  NumberExprAST(SourceLocation loc, double val)
    : ExprAST(loc), Val(val) {}
  virtual Value *Codegen();
  virtual Type *Typecheck();
};

class VariableExprAST : public ExprAST {
  std::string Name;
public:
  VariableExprAST(SourceLocation loc, const std::string &name)
    : ExprAST(loc), Name(name) {}
  virtual Value *Codegen();
  virtual Type *Typecheck();
};

class BinaryExprAST : public ExprAST {
  char Op;
  ExprAST *LHS, *RHS;
public:
  BinaryExprAST(SourceLocation loc, char op, ExprAST *lhs, ExprAST *rhs)
    : ExprAST(loc), Op(op), LHS(lhs), RHS(rhs) {}
  virtual Value *Codegen();
  virtual Type *Typecheck();
};

class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<ExprAST*> Args;
public:
  CallExprAST(SourceLocation loc, const std::string &callee, std::vector<ExprAST*> &args)
    : ExprAST(loc), Callee(callee), Args(args) {}
  virtual Value *Codegen();
  virtual Type *Typecheck();
};

class PrototypeAST {
  SourceLocation Location;

  std::string Name;
  std::string Returns;
  std::vector<std::string> ArgTypes;
  std::vector<std::string> ArgNames;
public:
  PrototypeAST(
    SourceLocation loc,
    const std::string &name,
    const std::string &returns,
    const std::vector<std::string> &argtypes,
    const std::vector<std::string> &argnames
  )
    : Location(loc), Name(name), Returns(returns), ArgTypes(argtypes), ArgNames(argnames) {}
  Function *Codegen();
  FunctionType *Typecheck();

  const std::string getName() { return Name; }
  const SourceLocation getLocation() { return Location; }
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
