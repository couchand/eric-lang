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
  SourceLocation l = getCurrentLocation();
  fprintf(stderr, "Error parsing at line %i column %i: %s\n", l.Line, l.Column, message);
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

// booleanexpr ::= "false" | "true"
static ExprAST *ParseBooleanExpr() {
  bool isTrue = tok_true == getCurrentToken();
  ExprAST *Result = new BooleanExprAST(getCurrentLocation(), isTrue);
  getNextToken(); // eat boolean
  return Result;
}

// integerexpr ::= integer
static ExprAST *ParseIntegerExpr() {
  ExprAST *Result = new IntegerExprAST(getCurrentLocation(), getIntegerVal());
  getNextToken(); // eat integer
  return Result;
}

// numberexpr ::= number
static ExprAST *ParseNumberExpr() {
  ExprAST *Result = new NumberExprAST(getCurrentLocation(), getNumberVal());
  getNextToken(); // eat number
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
  SourceLocation loc = getCurrentLocation();

  getNextToken(); // eat the identifier

  // simple variable reference
  if (CurTok != '(')
    return new VariableExprAST(loc, IdName);

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

  return new CallExprAST(loc, IdName, Args);
}

// blockexpr ::= expression+
static ExprAST *ParseBlockExpr() {
  if ('{' != getCurrentToken()) {
    return Error("Expecting { to start block");
  }
  getNextToken(); // eat '{'

  std::vector<ExprAST *> statements;
  int tok;
  while (1) {
    statements.push_back(ParseExpression());

    if (!statements.back()) {
      return 0;
    }

    tok = getCurrentToken();

    if (tok == '}') {
      getNextToken(); // eat }
      break;
    }
    if (tok == -1) {
      return Error("Expecting } to end block");
    }
  }

  return new BlockExprAST(getCurrentLocation(), statements);
}

// conditionalexpr ::= 'if' expression expression 'else' expression
static ExprAST *ParseConditionalExpr() {
  if (tok_if != getCurrentToken()) {
    return Error("Expecting 'if' to start conditional");
  }
  getNextToken(); // eat 'if'

  ExprAST *condition = ParseExpression();
  if (!condition) return 0;

  ExprAST *consequent = ParseExpression();
  if (!consequent) return 0;

  if (tok_else != getCurrentToken()) {
    return Error("Expecting 'else' in conditional");
  }
  getNextToken(); // eat 'else'

  ExprAST *alternate = ParseExpression();
  if (!alternate) return 0;

  return new ConditionalExprAST(getCurrentLocation(), condition, consequent, alternate);
}

// primary
//    ::= identifierexpr
//    ::= integerexpr
//    ::= numberexpr
//    ::= conditionalexpr
//    ::= parenexpr
//    ::= blockexpr
static ExprAST *ParsePrimary() {
  switch (CurTok) {
  default: return Error("expecting a primary expression");

  case tok_identifier:  return ParseIdentifierExpr();
  case tok_integer:     return ParseIntegerExpr();
  case tok_number:      return ParseNumberExpr();
  case tok_if:          return ParseConditionalExpr();

  case tok_false:
  case tok_true:
    return ParseBooleanExpr();

  case '(':             return ParseParenExpr();
  case '{':             return ParseBlockExpr();
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
  BinopPrecedence['='] = 10;
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['%'] = 35;
  BinopPrecedence['*'] = 40;
  BinopPrecedence['/'] = 40;
}

// binoprhs := (op primary)*
static ExprAST *ParseBinOpRHS(int ExprPrec, ExprAST *LHS) {
  while (1) {
    int TokPrec = GetTokPrecedence();

    if (TokPrec < ExprPrec) return LHS;

    int BinOp = CurTok;
    SourceLocation loc = getCurrentLocation();
    getNextToken(); // eat op

    ExprAST *RHS = ParsePrimary();
    if (!RHS) return 0;

    int NextPrec = GetTokPrecedence();
    if (TokPrec < NextPrec) {
      RHS = ParseBinOpRHS(TokPrec + 1, RHS);
      if (!RHS) return 0;
    }

    LHS = new BinaryExprAST(loc, BinOp, LHS, RHS);
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
  SourceLocation loc = getCurrentLocation();

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

  std::string Returns;
  if (getNextToken() == tok_void) {
    Returns = "void";
  }
  else {
    if (getCurrentToken() != tok_identifier)
      return ErrorP("Expected void or return type in prototype");

    Returns = getIdentifierStr();
  }

  if (getNextToken() != tok_identifier)
    return ErrorP("Expected function name in prototype");

  std::string FnName = getIdentifierStr();

  getNextToken(); // eat name

  return new PrototypeAST(loc, FnName, Returns, ArgTypes, ArgNames);
}

// top level expr
FunctionAST* ParseTopLevelExpr() {
  SourceLocation loc = getCurrentLocation();

  ExprAST *E = ParseExpression();
  if (!E) return 0;

  TypeData *T = E->Typecheck();
  if (!T) return 0;

  // stick in an anonymous function
  PrototypeAST *Proto = new PrototypeAST(loc, "", T->getName(), std::vector<std::string>(), std::vector<std::string>());
  return new FunctionAST(Proto, E);
}

FunctionAST *ParseFunctionDefinition() {
  SourceLocation loc = getCurrentLocation();
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
