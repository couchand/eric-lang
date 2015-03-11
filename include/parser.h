// parser

#ifndef _PARSER_H
#define _PARSER_H

#include "ast.h"

int getCurrentToken();
int getNextToken();

FunctionAST *ParseTopLevelExpr();
FunctionAST *ParseFunctionDefinition();
PrototypeAST *ParseExternalDeclaration();
ValueTypeAST *ParseValueTypeDefinition();

void InstallDefaultPrecedence();

#endif
