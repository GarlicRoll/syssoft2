#ifndef SYSSOFT_PARSER_H
#define SYSSOFT_PARSER_H

#include <stdlib.h>
#include <stdio.h>
#include "node.h"

typedef struct ChildNodes ChildNodes;
typedef struct TreeNode TreeNode;
typedef struct ChildNodes ChildNodes;
typedef struct ParseResult ParseResult;

extern TreeNode **allNodes;
extern int allNodesCount;
extern char** errors;
extern int errorsCount;

extern int yyparse();

extern FILE *yyin;

struct ParseResult {
    int size;
    TreeNode **nodes;
    char** errors;
    int errorsCount;
};

ParseResult* parse(FILE *file);
void freeMem(ParseResult *parseResult);

#endif //SYSSOFT_PARSER_H
