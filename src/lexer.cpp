// lexer

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "lexer.h"

static std::string IdentifierStr;   // only valid if tok_identifier
static double NumberVal;            // only valid if tok_number
static int IntegerVal;              // only valid if tok_integer

int gettok() {

  static int LastChar = ' ';

  if (LastChar == EOF)
    return tok_eof;

  while (isspace(LastChar))
    LastChar = getchar();

  if (isalpha(LastChar)) {
    IdentifierStr = LastChar;
    while (isalnum((LastChar = getchar())))
      IdentifierStr += LastChar;

    if (IdentifierStr == "value") return tok_value;
    if (IdentifierStr == "entity") return tok_entity;
    if (IdentifierStr == "external") return tok_external;
    if (IdentifierStr == "function") return tok_function;

    return tok_identifier;
  }

  if (isdigit(LastChar) || LastChar == '.') {
    bool isDouble = false;

    std::string NumStr;
    do {
      isDouble = isDouble || LastChar == '.';

      NumStr += LastChar;
      LastChar = getchar();
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
      LastChar = getchar();
    } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

    if (LastChar != EOF)
      return gettok();
  }

  if (LastChar == EOF)
    return tok_eof;

  int ThisChar = LastChar;
  LastChar = getchar();
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
