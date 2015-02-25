// lexer

#ifndef _LEXER_H
#define _LEXER_H

#include <string>

enum Token {

  // EOF
  tok_eof = -1,

  // structure declarations
  tok_value = -2, tok_entity = -3,

  // function declarations
  tok_external = -4, tok_function = -5,

  // identifiers
  tok_identifier = -6,

  // numbers
  tok_number = -7, tok_integer = -8,

  // boolean literals
  tok_false = -9, tok_true = -10,

};

int gettok();

const std::string getIdentifierStr();
double getNumberVal();
int getIntegerVal();

typedef struct T_SourceLocation {

  int Line;
  int Column;

} SourceLocation;

SourceLocation getCurrentLocation();

void InitializeLexer();

#endif
