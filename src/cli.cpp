// cli

#include <cstdio>

#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "typecheck.h"
#include "builtins.h"

static bool showPrompt = true;

static void prompt() {
  if (showPrompt) fprintf(stderr, "ready> ");
}

static void prime() {
  prompt();
  getNextToken();
}

std::vector<ExprAST*> TopLevelExpressions;

static Function* handleTopLevelExpression() {
  ExprAST *line = ParseTopLevelExpr();
  if (line) {
    TypeData *T = line->Typecheck();
    if (!T) return 0;

    TopLevelExpressions.push_back(line);

    if (!showPrompt) return 0;

    SourceLocation loc = getCurrentLocation();

    TypeSpecifier *ts = new BasicTypeSpecifier(T->getName());

    // stick in an anonymous function
    PrototypeAST *Proto = new PrototypeAST(loc, "", ts, std::vector<TypeSpecifier *>(), std::vector<std::string>());
    FunctionAST *anonymous = new FunctionAST(Proto, line);

    return anonymous->Codegen();
  }
  else {
    getNextToken();
  }
  return 0;
}

static Function* handleFunctionDefinition() {
  FunctionAST *block = ParseFunctionDefinition();
  if (block) {
    TypeData *T = block->Typecheck();
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
    return proto->Codegen();
  }
  else {
    getNextToken();
  }
  return 0;
}

static void handleValueTypeDefinition() {
  ValueTypeAST *valueType = ParseValueTypeDefinition();

  if (valueType) {
    StructTypeData *t = (StructTypeData *)valueType->MakeType();

    //fprintf(stdout, "New type %s created\n", t->getName().c_str());

    for (unsigned i = 0, e = t->getNumFields(); i < e; i++) {
      TypeData *ft = t->getFieldType(i);
      if (!ft) {
        fprintf(stdout, "err on field %i\n", i);
        return;
      }
      //fprintf(stdout, "  field %i type %s\n", i, ft->getName().c_str());
    }
  }
  else {
    getNextToken();
  }
}

static void handleNext(int token) {
  Function* code;
  switch (token) {
    default:            code = handleTopLevelExpression(); break;
    case tok_function:  code = handleFunctionDefinition(); break;
    case tok_external:  code = handleExternalDeclaration(); break;
    case tok_value:     handleValueTypeDefinition(); break;
  }

  if (code && showPrompt) {
    code->dump();
  }

  prompt();
}

static void mainLoop() {
  while (1) {
    int tok = getCurrentToken();
    switch (tok) {
    default:            handleNext(tok); break;
    case ';':           getNextToken(); break;
    case tok_eof:       return;
    }
  }
}

int main(int argc, char** argv) {
  std::string flag = "-c";
  std::string filename = "a.eric";
  if (argc > 1 && flag == argv[1]) {
    showPrompt = false;

    if (argc > 2) filename = argv[2];
  }

  InitializeLexer();
  InstallDefaultPrecedence();

  prime();

  InitializeCodegen(filename.c_str());
  InitializeTypecheck();
  InitializeBuiltins();

  mainLoop();

  CreateMainFunction(TopLevelExpressions);

  if (showPrompt) fprintf(stderr, "\n\n\n");

  DumpAllCode();

  return 0;
}
