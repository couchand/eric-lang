// builtins

#include <vector>

#include "ast.h"

static Function *initializeMalloc() {
  SourceLocation loc = { 0, 0 };

  std::string returnType = "[byte]";

  std::vector<std::string> argTypes;
  std::vector<std::string> argNames;
  argTypes.push_back("integer");
  argNames.push_back("size");

  PrototypeAST *proto = new PrototypeAST(loc, "malloc", returnType, argTypes, argNames);

  return proto->Codegen();
}

static Function *initializeLength(std::string elType) {
  SourceLocation loc = { 0, 0 };

  std::string functionName = "length.";
  functionName += elType;

  std::string returnType = "integer";

  std::vector<std::string> argTypes;
  std::vector<std::string> argNames;

  std::string typeName = "[";
  typeName += elType;
  typeName += "]";

  argTypes.push_back(typeName);
  argNames.push_back("array");

  PrototypeAST *proto = new PrototypeAST(loc, functionName, returnType, argTypes, argNames);

//  ExprAST *body = new 
  return 0;
}

void InitializeBuiltins() {
  initializeMalloc();
}
