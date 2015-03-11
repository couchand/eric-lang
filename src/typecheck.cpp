// typecheck

#include <cstdio>
#include <map>

#include "ast.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/TypeBuilder.h"

void TypeError(SourceLocation loc, const char *message) {
  fprintf(stderr, "Error while typechecking at line %i, column %i: %s\n", loc.Line, loc.Column, message);
}

TypeData *ErrorT(ExprAST *e, const char *message) {
  TypeError(e->getLocation(), message);
  return 0;
}

FunctionTypeData *ErrorFT(SourceLocation loc, const char *message) {
  TypeError(loc, message);
  return 0;
}

static std::map<std::string, TypeData *> NamedValueTypes;

void InitializeTypecheck() {
  LLVMContext &Context = getGlobalContext();
}

TypeData *BooleanExprAST::Typecheck() {
  return TypeData::getType("boolean");
}

TypeData *IntegerExprAST::Typecheck() {
  return TypeData::getType("integer");
}

TypeData *NumberExprAST::Typecheck() {
  return TypeData::getType("number");
}

TypeData *VariableExprAST::Typecheck() {
  TypeData* T = NamedValueTypes[Name];
  if (!T) {
    std::string message = "Unknown variable name: ";
    message += Name;
    return ErrorT(this, message.c_str());
  }

  return T;
}

TypeData *makeCompatible(TypeData *a, TypeData *b) {
  // TODO: conversions
  return a == b ? a : 0;
}

TypeData *BinaryExprAST::Typecheck() {
  TypeData *L = LHS->Typecheck();
  TypeData *R = RHS->Typecheck();
  if (!L || !R) return 0;

  TypeData *Combined = makeCompatible(L, R);
  if (!Combined) {
    std::string message = "Incompatible binary expression types in: ";
    message += Op;
    return ErrorT(this, message.c_str());
  }

  switch (Op) {
  default: return Combined;
  case '<':
  case '>':
  case '=':
  case '&':
  case '|': return TypeData::getType("boolean");
  }
}

TypeData *CallExprAST::Typecheck() {
  if (Callee == "integer" || Callee == "number" || Callee == "boolean") {
    if (Args.size() != 1) {
      std::string message = "Cast to ";
      message += Callee;
      message += " expects a single parameter";
      return ErrorT(this, message.c_str());
    }

    TypeData *argType = Args[0]->Typecheck();

    std::string typeslug = "(";
    typeslug += argType->getName();
    typeslug += ")";
    typeslug += Callee;

    TypeData *fnType = TypeData::getType(typeslug.c_str());
    if (!fnType) {
      std::string message = "Unable to find cast for ";
      message += typeslug;
      return ErrorT(this, message.c_str());
    }

    return TypeData::getType(Callee);
  }

  FunctionTypeData* FT = FunctionTypeData::getFunctionType(Callee);

  if (!FT) {
    std::string message = "Unknown function reference: ";
    message += Callee;
    return ErrorT(this, message.c_str());
  }

  if (FT->getNumParameters() != Args.size())
    return ErrorT(this, "Wrong number of arguments to function");

  for (unsigned i = 0, e = Args.size(); i < e; i++) {
    TypeData *argType = Args[i]->Typecheck();
    if (!argType) return 0;

    TypeData *paramType = FT->getParameterType(i);
    TypeData *Coalesced = makeCompatible(paramType, argType);
    if (Coalesced != paramType) {
      std::string message = "Incompatible types in call to: ";
      message += Callee;
      return ErrorT(this, message.c_str());
    }
  }

  return FT->getReturnType();
}

TypeData *ValueLiteralAST::Typecheck() {
  TypeData *valueType = TypeData::getType(ValueType);
  if (!valueType) {
    std::string message = "No type found named ";
    message += ValueType;
    return ErrorT(this, message.c_str());
  }

  if (!valueType->isStructType())
    return ErrorT(this, "Expected a structure type");

  StructTypeData *st = (StructTypeData *)valueType;

  if (Fields.size() != st->getNumFields())
    return ErrorT(this, "Wrong number of fields in structure literal");

  for (unsigned i = 0, e = Fields.size(); i < e; i++) {
    TypeData *literalType = Fields[i]->Typecheck();
    if (!literalType) return 0;

    TypeData *expected = st->getFieldType(i);
    TypeData *coalesced = makeCompatible(expected, literalType);
    if (coalesced != expected) {
      std::string message = "Incomaptible type in ";
      message += ValueType;
      message += " literal";
      return ErrorT(this, message.c_str());
    }
  }

  //fprintf(stdout, "type check for %s literal successful\n", ValueType.c_str());

  return valueType;
}

TypeData *ValueReferenceAST::Typecheck() {
  ReferenceTypes.clear();

  TypeData *ref = NamedValueTypes[Name];
  if (!ref) {
    std::string message = "Unknown structure name: ";
    message += Name;
    return ErrorT(this, message.c_str());
  }

  std::string soFar = Name;

  for (unsigned i = 0, e = References.size(); i < e; i++) {

    if (!ref->isStructType()) {
      std::string message = "Variable ";
      message += soFar;
      message += " is not a structure type";
      return ErrorT(this, message.c_str());
    }

    StructTypeData *st = (StructTypeData *)ref;

    ReferenceTypes.push_back(st);

    ref = st->getFieldType(References[i]);

    if (!ref) {
      std::string message = "Variable ";
      message += soFar;
      message += " has no field named ";
      message += References[i];
      return ErrorT(this, message.c_str());
    }

    soFar += "." + References[i];
  }

  return ref;
}

TypeData *BlockExprAST::Typecheck() {
  if (Statements.size() == 0) {
    return ErrorT(this, "Block should have at least one statement");
  }
  TypeData *t;
  for (int i = 0, e = Statements.size(); i < e; i++) {
    t = Statements[i]->Typecheck();

    if (!t) return 0;
  }
  return t;
}

TypeData *ConditionalExprAST::Typecheck() {
  TypeData *conditionType = Condition->Typecheck();
  if (!conditionType) return 0;
  if (conditionType != TypeData::getType("boolean")) {
    return ErrorT(this, "Condition should be boolean type");
  }

  TypeData *consequentType = Consequent->Typecheck();
  if (!consequentType) return 0;

  TypeData *alternateType = Alternate->Typecheck();
  if (!alternateType) return 0;

  TypeData *coalesced = makeCompatible(consequentType, alternateType);
  if (!coalesced) {
    return ErrorT(this, "Incompatible types in condition");
  }

  return coalesced;
}

FunctionTypeData *PrototypeAST::Typecheck() {
  TypeData *ReturnType;
  if (Returns != "") {
    ReturnType = TypeData::getType(Returns);
    if (!ReturnType) {
      std::string message = "Unknown return type in function prototype: ";
      message += Returns;
      return ErrorFT(Location, message.c_str());
    }
  }

  std::vector<TypeData *> Params;
  for (unsigned i = 0, e = ArgTypes.size(); i < e; i++) {
    TypeData *ArgType = TypeData::getType(ArgTypes[i]);
    if (!ArgType) {
      std::string message = "Unknown param type in function prototype: ";
      message += ArgTypes[i];
      return ErrorFT(Location, message.c_str());
    }

    Params.push_back(ArgType);
    NamedValueTypes[ArgNames[i]] = ArgType;
  }

  FunctionTypeData *FT = new FunctionTypeData(ReturnType, Params);

  FunctionTypeData::registerFunctionType(Name, FT);

  return FT;
}

FunctionTypeData *FunctionAST::Typecheck() {
  NamedValueTypes.clear();

  FunctionTypeData *T = Proto->Typecheck();
  if (!T) return 0;

  TypeData* BodyType = Body->Typecheck();
  if (!BodyType) return 0;

  TypeData* ReturnType = T->getReturnType();

  if (ReturnType->getName() == "void")
    return T;

  TypeData* Coalesced = makeCompatible(ReturnType, BodyType);
  if (!Coalesced) {
    std::string message = "Incompatible types in definition of: ";
    message += Proto->getName();
    return ErrorFT(Proto->getLocation(), message.c_str());
  }

  return T;
}
