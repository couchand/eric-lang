// ast

#ifndef _AST_H
#define _AST_H

#include <string>
#include <vector>

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Module.h"

#include "lexer.h"
#include "types.h"

using namespace llvm;

class ExprAST {
  SourceLocation Location;

public:
  virtual ~ExprAST() {}
  virtual Value *Codegen() = 0;
  virtual TypeData *Typecheck() = 0;

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
  virtual TypeData *Typecheck();
};

class IntegerExprAST : public ExprAST {
  int Val;
public:
  IntegerExprAST(SourceLocation loc, int val)
    : ExprAST(loc), Val(val) {}
  virtual Value *Codegen();
  virtual TypeData *Typecheck();
};

class NumberExprAST : public ExprAST {
  double Val;
public:
  NumberExprAST(SourceLocation loc, double val)
    : ExprAST(loc), Val(val) {}
  virtual Value *Codegen();
  virtual TypeData *Typecheck();
};

class VariableExprAST : public ExprAST {
  std::string Name;
public:
  VariableExprAST(SourceLocation loc, const std::string &name)
    : ExprAST(loc), Name(name) {}
  virtual Value *Codegen();
  virtual TypeData *Typecheck();
};

class BinaryExprAST : public ExprAST {
  char Op;
  ExprAST *LHS, *RHS;
public:
  BinaryExprAST(SourceLocation loc, char op, ExprAST *lhs, ExprAST *rhs)
    : ExprAST(loc), Op(op), LHS(lhs), RHS(rhs) {}
  virtual Value *Codegen();
  virtual TypeData *Typecheck();
};

class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<ExprAST*> Args;
public:
  CallExprAST(SourceLocation loc, const std::string &callee, const std::vector<ExprAST*> &args)
    : ExprAST(loc), Callee(callee), Args(args) {}
  virtual Value *Codegen();
  virtual TypeData *Typecheck();
};

class ValueLiteralAST : public ExprAST {
  std::string ValueType;
  std::vector<ExprAST*> Fields;
public:
  ValueLiteralAST(SourceLocation loc, const std::string &type, const std::vector<ExprAST*> &fields)
    : ExprAST(loc), ValueType(type), Fields(fields) {}
  virtual Value *Codegen();
  virtual TypeData *Typecheck();
};

class ValueReferenceAST : public ExprAST {
  std::string Name;
  std::vector<std::string> References;
  std::vector<StructTypeData *> ReferenceTypes;
public:
  ValueReferenceAST(SourceLocation loc, const std::string &name, const std::vector<std::string> &refs)
    : ExprAST(loc), Name(name), References(refs) {}
  virtual Value *Codegen();
  virtual TypeData *Typecheck();
};

class BlockExprAST : public ExprAST {
  std::vector<ExprAST *> Statements;
public:
  BlockExprAST(SourceLocation loc, std::vector<ExprAST *> &statements)
    : ExprAST(loc), Statements(statements) {}
  virtual Value *Codegen();
  virtual TypeData *Typecheck();
};

class ConditionalExprAST : public ExprAST {
  ExprAST *Condition;
  ExprAST *Consequent;
  ExprAST *Alternate;
public:
  ConditionalExprAST(SourceLocation loc, ExprAST *cond, ExprAST *cons, ExprAST *alt)
    : ExprAST(loc), Condition(cond), Consequent(cons), Alternate(alt) {}
  virtual Value *Codegen();
  virtual TypeData *Typecheck();
};

class ValueTypeAST {
  SourceLocation Location;

  std::string Name;
  std::vector<std::string> ElementTypes;
  std::vector<std::string> ElementNames;
public:
  ValueTypeAST(
    SourceLocation loc,
    const std::string &name,
    const std::vector<std::string> &eltypes,
    const std::vector<std::string> &elnames
  ) : Location(loc), Name(name), ElementTypes(eltypes), ElementNames(elnames) {}

  TypeData *MakeType();
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
  FunctionTypeData *Typecheck();

  void UpdateArguments(Function *F);

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
  FunctionTypeData *Typecheck();
};

#endif
