// typecheck

#include <cstdio>
#include <map>

#include "ast.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/TypeBuilder.h"

void TypeError(SourceLocation loc, const char *message) {
  fprintf(stderr, "Error while typechecking at line %i, column %i: %s\n", loc.Line, loc.Column, message);
}

Type *ErrorT(ExprAST *e, const char *message) {
  TypeError(e->getLocation(), message);
  return 0;
}

FunctionType *ErrorFT(SourceLocation loc, const char *message) {
  TypeError(loc, message);
  return 0;
}

static std::map<std::string, Type*> NamedTypes;
static std::map<std::string, Type*> NamedValueTypes;
static std::map<std::string, FunctionType*> FunctionTypes;

void InitializeTypecheck() {
  LLVMContext &Context = getGlobalContext();

  NamedTypes["number"] = TypeBuilder<llvm::types::ieee_double, true>::get(Context);
  NamedTypes["integer"] = TypeBuilder<llvm::types::i<64>, true>::get(Context);

  FunctionTypes["number"] = TypeBuilder<llvm::types::ieee_double(llvm::types::i<64>), true>::get(Context);
  FunctionTypes["integer"] = TypeBuilder<llvm::types::i<64>(llvm::types::ieee_double), true>::get(Context);
}

Type *IntegerExprAST::Typecheck() {
  return NamedTypes["integer"];
}

Type *NumberExprAST::Typecheck() {
  return NamedTypes["number"];
}

Type *VariableExprAST::Typecheck() {
  Type* T = NamedValueTypes[Name];
  if (!T) {
    std::string message = "Unknown variable name: ";
    message += Name;
    return ErrorT(this, message.c_str());
  }

  return T;
}

Type *makeCompatible(Type *a, Type *b) {
  return a == b ? a : 0;
/*
  bool aInt = a->isIntegerTy();
  bool bInt = b->isIntegerTy();

  if (aInt && bInt) {
    unsigned aWidth = a->getIntegerBitWidth();
    unsigned bWidth = b->getIntegerBitWidth();
    if (aWidth < bWidth) {
      // success: cast a to b
      return b;
    }
    else {
      // success: cast b to a
      return a;
    }
  }

  if (aInt) {
    if (b->isFloatingPointTy()) {
      // success: cast a to b
      return b;
    }

    // error: a integer, b nonnumeric
    return 0;
  }

  if (bInt) {
    if (a->isFloatingPointTy()) {
      // success: cast b to a
      return a;
    }

    // error: b integer, a nonnumeric
    return 0;
  }

  bool aFloat = a->isFloatingPointTy();
  bool bFloat = b->isFloatingPointTy();

  if (aFloat && bFloat) {
    // assume for now there's only one floating point type (TODO)
    return a;
  }

  return 0;
*/
}

Type *BinaryExprAST::Typecheck() {
  Type *L = LHS->Typecheck();
  Type *R = RHS->Typecheck();
  if (!L || !R) return 0;

  Type *Combined = makeCompatible(L, R);
  if (!Combined) {
    std::string message = "Incompatible binary expression types in: ";
    message += Op;
    return ErrorT(this, message.c_str());
  }

  return Combined;
}

Type *CallExprAST::Typecheck() {
  FunctionType* FT = FunctionTypes[Callee];

  if (!FT) {
    std::string message = "Unknown function reference: ";
    message += Callee;
    return ErrorT(this, message.c_str());
  }

  if (FT->getNumParams() != Args.size())
    return ErrorT(this, "Wrong number of arguments to function");

  for (unsigned i = 0, e = Args.size(); i < e; i++) {
    Type *argType = Args[i]->Typecheck();
    if (!argType) return 0;

    Type *paramType = FT->getParamType(i);
    Type *Coalesced = makeCompatible(paramType, argType);
    if (Coalesced != paramType) {
      std::string message = "Incompatible types in call to: ";
      message += Callee;
      return ErrorT(this, message.c_str());
    }
  }

  return FT->getReturnType();
}

FunctionType *PrototypeAST::Typecheck() {
  Type *ReturnType = NamedTypes[Returns];
  if (!ReturnType) {
    std::string message = "Unknown return type in function prototype: ";
    message += Returns;
    return ErrorFT(Location, message.c_str());
  }

  std::vector<Type *> Params;
  for (unsigned i = 0, e = ArgTypes.size(); i < e; i++) {
    Type *ArgType = NamedTypes[ArgTypes[i]];
    if (!ArgType) {
      std::string message = "Unknown param type in function prototype: ";
      message += ArgTypes[i];
      return ErrorFT(Location, message.c_str());
    }

    Params.push_back(ArgType);
    NamedValueTypes[ArgNames[i]] = ArgType;
  }

  FunctionType *FT = FunctionType::get(ReturnType, Params, false);

  FunctionTypes[Name] = FT;

  return FT;
}

FunctionType *FunctionAST::Typecheck() {
  NamedValueTypes.clear();

  FunctionType *T = Proto->Typecheck();
  if (!T) return 0;

  Type* BodyType = Body->Typecheck();
  if (!BodyType) return 0;

  Type* ReturnType = T->getReturnType();
  Type* Coalesced = makeCompatible(ReturnType, BodyType);
  if (!Coalesced) {
    std::string message = "Incompatible types in definition of: ";
    message += Proto->getName();
    return ErrorFT(Proto->getLocation(), message.c_str());
  }

  return T;
}
