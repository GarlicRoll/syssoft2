#ifndef SYSSOFT_CFG_H
#define SYSSOFT_CFG_H

#include "parser.h"

typedef struct FilenameParseTree FilenameParseTree;
typedef struct CFGNode CFGNode;
typedef struct FunExecution FunExecution;
typedef struct Array Array;
typedef struct ListingNode ListingNode;

struct CFGNode {
    char *text;
    CFGNode *definitely; // безусловный переход
    CFGNode *conditionally; // условный переход
    TreeNode *operationTree;
    int id;
    int printed;
    ListingNode *listingNode;
};

struct Array {
    int size;
    int nextPosition;
    void **elements;
};

struct FunExecution {
    char *name;
    char *filename;
    TreeNode *signature;
    TreeNode *funCalls;
    CFGNode *nodes;
    char **errors;
    int errorsCount;
};

struct FilenameParseTree {
    char *filename;
    ParseResult *tree;
};

Array *executionGraph(FilenameParseTree *input, int size);

void printExecution(FunExecution *funExecution, FILE *outputFunCallFile, FILE *outputOperationTreesFile,
                    FILE *outputExecutionFile);
void addToList(Array *currentArray, void *element);

#endif
