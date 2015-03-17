// builtins

#include <vector>

#include "ast.h"

static Function *initializeMalloc() {
  SourceLocation loc = { 0, 0 };

  TypeSpecifier *returnType = new ArrayTypeSpecifier(new BasicTypeSpecifier("byte"));

  std::vector<TypeSpecifier *> argTypes;
  std::vector<std::string> argNames;
  argTypes.push_back(new BasicTypeSpecifier("integer"));
  argNames.push_back("size");

  PrototypeAST *proto = new PrototypeAST(loc, "malloc", returnType, argTypes, argNames);

  return proto->Codegen();
}

static Function *initializeLength(std::string elType) {
  SourceLocation loc = { 0, 0 };

  std::string functionName = "length.";
  functionName += elType;

  TypeSpecifier *returnType = new BasicTypeSpecifier("integer");

  std::vector<TypeSpecifier *> argTypes;
  std::vector<std::string> argNames;

  argTypes.push_back(new ArrayTypeSpecifier(new BasicTypeSpecifier(elType)));
  argNames.push_back("array");

  PrototypeAST *proto = new PrototypeAST(loc, functionName, returnType, argTypes, argNames);

//  ExprAST *body = new 
  return 0;
}

void InitializeBuiltins() {
  initializeMalloc();
}
