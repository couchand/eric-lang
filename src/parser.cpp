// parser

#include "lexer.h"
#include "parser.h"

#include <cstdio>
#include <map>
#include <vector>

// single token lookahead

static int CurTok;

int getCurrentToken() {
  return CurTok;
}

int getNextToken() {
  return CurTok = gettok();
}

// basic error reporting

ExprAST *Error(const char *message) {
  fprintf(stderr, "Error: %s\n", message);
  return 0;
}

PrototypeAST *ErrorP(const char *message) {
  Error(message);
  return 0;
}

FunctionAST *ErrorF(const char* message) {
  Error(message);
  return 0;
}

// recursive descent parsing

static ExprAST* ParseExpression();

// numberexpr ::= number
static ExprAST *ParseNumberExpr() {
  ExprAST *Result = new NumberExprAST(getNumberVal());
  getNextToken();
  return Result;
}

// parenexpr ::= '(' expression ')'
static ExprAST* ParseParenExpr() {
  getNextToken(); // eat (

  ExprAST *V = ParseExpression();
  if (!V) return 0;

  if (CurTok != ')')
    return Error("expected ')' in paren expression");

  getNextToken(); // eat )

  return V;
}

// identifierexpr
//    ::= identifier
//    ::= identifier '(' expression* ')'
static ExprAST *ParseIdentifierExpr() {
  std::string IdName = getIdentifierStr();

  getNextToken(); // eat the identifier

  // simple variable reference
  if (CurTok != '(')
    return new VariableExprAST(IdName);

  // function call
  getNextToken(); // eat (

  std::vector<ExprAST*> Args;
  if (CurTok != ')') {
    while (1) {

      ExprAST *Arg = ParseExpression();
      if (!Arg) return 0;

      Args.push_back(Arg);

      if (CurTok == ')') break;

      if (CurTok != ',')
        return Error("Expected ')' or ',' in argument list");

      getNextToken(); // eat ,

    }
  }

  getNextToken(); // eat )

  return new CallExprAST(IdName, Args);
}

// primary
//    ::= identifierexpr
//    ::= numberexpr
//    ::= parenexpr
static ExprAST *ParsePrimary() {
  switch (CurTok) {
  default: return Error("expecting a primary expression");
  case tok_identifier:  return ParseIdentifierExpr();
  case tok_number:      return ParseNumberExpr();
  case '(':             return ParseParenExpr();
  }
}

// binary operators
static std::map<char, int> BinopPrecedence;

static int GetTokPrecedence() {
  if (!isascii(CurTok)) return -1;

  int TokPrec = BinopPrecedence[CurTok];
  if (TokPrec <= 0) return -1;

  return TokPrec;
}

void InstallDefaultPrecedence() {
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;
}

// binoprhs := (op primary)*
static ExprAST *ParseBinOpRHS(int ExprPrec, ExprAST *LHS) {
  while (1) {
    int TokPrec = GetTokPrecedence();

    if (TokPrec < ExprPrec) return LHS;

    int BinOp = CurTok;
    getNextToken(); // eat op

    ExprAST *RHS = ParsePrimary();
    if (!RHS) return 0;

    int NextPrec = GetTokPrecedence();
    if (TokPrec < NextPrec) {
      RHS = ParseBinOpRHS(TokPrec + 1, RHS);
      if (!RHS) return 0;
    }

    LHS = new BinaryExprAST(BinOp, LHS, RHS);
  }
}

// expression ::= primary binoprhs
static ExprAST *ParseExpression() {
  ExprAST *LHS = ParsePrimary();
  if (!LHS) return 0;

  return ParseBinOpRHS(0, LHS);
}

// prototype ::= '(' (id id (',' id id)*) ')' id id
static PrototypeAST *ParsePrototype() {
  if (getCurrentToken() != '(')
    return ErrorP("Expected parameter list in prototype");

  getNextToken(); // eat (

  std::vector<std::string> ArgTypes;
  std::vector<std::string> ArgNames;

  while (getCurrentToken() != ')') {

    if (getCurrentToken() != tok_identifier)
      return ErrorP("Expected parameter type in prototype");

    ArgTypes.push_back(getIdentifierStr());

    if (getNextToken() != tok_identifier)
      return ErrorP("Expected parameter name in prototype");

    ArgNames.push_back(getIdentifierStr());

    if (getNextToken() == ')') break;

    if (getCurrentToken() != ',')
      return ErrorP("Expected ',' or ')' in parameter list");

    getNextToken(); // eat ,
  }

  if (getNextToken() != tok_identifier)
    return ErrorP("Expected return type in prototype");

  std::string Returns = getIdentifierStr();

  if (getNextToken() != tok_identifier)
    return ErrorP("Expected function name in prototype");

  std::string FnName = getIdentifierStr();

  getNextToken(); // eat name

  return new PrototypeAST(FnName, Returns, ArgTypes, ArgNames);
}

// top level expr
FunctionAST* ParseTopLevelExpr() {
  if (ExprAST *E = ParseExpression()) {
    // stick in an anonymous function
    PrototypeAST *Proto = new PrototypeAST("repl", "void", std::vector<std::string>(), std::vector<std::string>());
    return new FunctionAST(Proto, E);
  }
  return 0;
}

FunctionAST *ParseFunctionDefinition() {
  getNextToken(); // eat function
  PrototypeAST *Proto = ParsePrototype();
  if (!Proto) return 0;

  ExprAST* Body = ParseExpression();
  if (!Body) return 0;

  return new FunctionAST(Proto, Body);
}

PrototypeAST *ParseExternalDeclaration() {
  getNextToken(); // eat external
  return ParsePrototype();
}
