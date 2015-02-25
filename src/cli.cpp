// cli

#include <cstdio>

#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "typecheck.h"

static void prompt() {
  fprintf(stderr, "ready> ");
}

static void prime() {
  prompt();
  getNextToken();
}

static Function* handleTopLevelExpression() {
  FunctionAST *block = ParseTopLevelExpr();
  if (block) {
    fprintf(stderr, "Parsed expression!\n");

    Type *T = block->Typecheck();
    if (!T) return 0;

    return block->Codegen();
  }
  else {
    getNextToken();
  }
  return 0;
}

static Function* handleFunctionDefinition() {
  FunctionAST *block = ParseFunctionDefinition();
  if (block) {
    fprintf(stderr, "Parsed function definition!\n");

    Type *T = block->Typecheck();
    if (!T) return 0;

    return block->Codegen();
  }
  else {
    getNextToken();
  }
  return 0;
}

static Function* handleExternalDeclaration() {
  PrototypeAST *proto = ParseExternalDeclaration();
  if (proto) {
    fprintf(stderr, "Parsed external declaration!\n");
    return proto->Codegen();
  }
  else {
    getNextToken();
  }
  return 0;
}

static void handleNext(int token) {
  Function* code;
  switch (token) {
    default:            code = handleTopLevelExpression(); break;
    case tok_function:  code = handleFunctionDefinition(); break;
    case tok_external:  code = handleExternalDeclaration(); break;
  }

  if (code) {
    code->dump();
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
  InitializeLexer();
  InstallDefaultPrecedence();

  prime();

  InitializeCodegen();
  InitializeTypecheck();

  mainLoop();

  fprintf(stderr, "\n\n\n");
  DumpAllCode();

  return 0;
}
