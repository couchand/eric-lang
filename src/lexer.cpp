// lexer

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

#include "lexer.h"

static std::string IdentifierStr;   // only valid if tok_identifier
static double NumberVal;            // only valid if tok_number
static int IntegerVal;              // only valid if tok_integer

static SourceLocation CurLoc;
static SourceLocation LexLoc = { 1, 0 };

SourceLocation getCurrentLocation() {
  return CurLoc;
}

bool isNewline(int ch) {
  return ch == '\n' || ch == '\r';
}

int advance() {

  int LastChar = getchar();

  if (isNewline(LastChar)) {
    LexLoc.Line++;
    LexLoc.Column = 0;
  } else {
    LexLoc.Column++;
  }

  return LastChar;

}

std::map<std::string, int> Keywords;

void InitializeLexer() {

  Keywords["value"]     = tok_value;
  Keywords["entity"]    = tok_entity;
  Keywords["external"]  = tok_external;
  Keywords["function"]  = tok_function;
  Keywords["false"]     = tok_false;
  Keywords["true"]      = tok_true;
  Keywords["void"]      = tok_void;
  Keywords["if"]        = tok_if;
  Keywords["else"]      = tok_else;

}

int gettok() {

  static int LastChar = ' ';

  if (LastChar == EOF)
    return tok_eof;

  while (isspace(LastChar))
    LastChar = advance();

  CurLoc = LexLoc;

  if (isalpha(LastChar)) {
    IdentifierStr = LastChar;
    while (isalnum((LastChar = advance())))
      IdentifierStr += LastChar;

    if (Keywords.count(IdentifierStr) > 0) {
      return Keywords[IdentifierStr];
    }

    return tok_identifier;
  }

  if (isdigit(LastChar)) {
    bool isDouble = false;

    std::string NumStr;
    do {
      isDouble = isDouble || LastChar == '.';

      NumStr += LastChar;
      LastChar = advance();
    } while (isdigit(LastChar) || LastChar == '.');

    if (isDouble) {
      NumberVal = strtod(NumStr.c_str(), 0);
      return tok_number;
    }
    IntegerVal = atoi(NumStr.c_str());
    return tok_integer;
  }

  if (LastChar == '#') {
    do {
      LastChar = advance();
    } while (LastChar != EOF && !isNewline(LastChar));

    if (LastChar != EOF)
      return gettok();
  }

  if (LastChar == EOF)
    return tok_eof;

  int ThisChar = LastChar;
  LastChar = advance();
  return ThisChar;

}

const std::string getIdentifierStr() {
  return IdentifierStr;
}

double getNumberVal() {
  return NumberVal;
}

int getIntegerVal() {
  return IntegerVal;
}
