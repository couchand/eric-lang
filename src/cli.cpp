// cli

#include <cstdio>

#include "lexer.h"
#include "parser.h"
#include "codegen.h"

static void prompt() {
  fprintf(stderr, "ready> ");
}

static void prime() {
  prompt();
  getNextToken();
}

static void handleTopLevelExpression() {
  FunctionAST *block = ParseTopLevelExpr();
  if (block) {
    fprintf(stderr, "Parsed expression!\n");

    Function *code = block->Codegen();

    if (code) {
      code->dump();
    }
  }
  else {
    getNextToken();
  }
}

static void handleFunctionDefinition() {
  FunctionAST *block = ParseFunctionDefinition();
  if (block) {
    fprintf(stderr, "Parsed function definition!\n");

    Function* code = block->Codegen();

    if (code) {
      code->dump();
    }
  }
  else {
    getNextToken();
  }
}

static void handleExternalDeclaration() {
  PrototypeAST *proto = ParseExternalDeclaration();
  if (proto) {
    fprintf(stderr, "Parsed external declaration!\n");

    Function *code = proto->Codegen();

    if (code) {
      code->dump();
    }
  }
  else {
    getNextToken();
  }
}

static void mainLoop() {
  while (1) {
    prompt();
    switch (getCurrentToken()) {
    default:            handleTopLevelExpression(); break;
    case tok_function:  handleFunctionDefinition(); break;
    case tok_external:  handleExternalDeclaration(); break;
    case ';':           getNextToken(); break;
    case tok_eof:       return;
    }
  }
}

int main() {
  InstallDefaultPrecedence();

  prime();

  InitializeCodegen();

  mainLoop();

  fprintf(stderr, "\n\n\n");
  DumpAllCode();

  return 0;
}
