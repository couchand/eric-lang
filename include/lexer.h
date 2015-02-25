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

  // identifiers and numbers
  tok_identifier = -6, tok_number = -7, tok_integer = -8,

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

#endif
