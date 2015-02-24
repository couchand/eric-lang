// lexer

#include <cctype>
#include <cstdio>
#include <cstdlib>

#include "lexer.h"

static std::string IdentifierStr;   // only valid if tok_identifier
static double NumberVal;            // only valid if tok_number

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
    std::string NumStr;
    do {
      NumStr += LastChar;
      LastChar = getchar();
    } while (isdigit(LastChar) || LastChar == '.');

    NumberVal = strtod(NumStr.c_str(), 0);
    return tok_number;
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
