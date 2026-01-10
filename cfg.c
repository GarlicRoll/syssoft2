#include "cfg.h"
#include <string.h>

typedef struct FunCalls FunCalls;

const int START_ARRAY_SIZE = 8;

struct FunCalls {
    Array *funCalls;
    char *currentFunName;
};

Array exceptions;
int currentExecutionId = -1;

CFGNode *executionNode(TreeNode *treeNode, CFGNode *nextNode,
                             CFGNode *breakNode, FunCalls *funCalls);

char *mallocString(char *text) {
    char *pointer = malloc(sizeof(char) * 1024);
    sprintf(pointer, "%s", text);
    return pointer;
}

// добавление в массив с возможным динамическим расширением
void addToList(Array *currentArray, void *element) {
    void **nodes;
    if (currentArray->size != currentArray->nextPosition) {
        nodes = currentArray->elements;
    } else {
        nodes = malloc(sizeof(void *) * 2 * currentArray->size);
        for (int i = 0; i < currentArray->size; ++i) {
            nodes[i] = currentArray->elements[i];
        }
        free(currentArray->elements);
        currentArray->elements = nodes;
    }
    nodes[currentArray->nextPosition] = element;
    currentArray->nextPosition += 1;
}

void addException(char *text) {
    char *exception = mallocString(text);
    addToList(&exceptions, exception);
}

int getNextExecutionId() {
    currentExecutionId++;
    return currentExecutionId;
}

// утилита для получения всех node дерева разбора в виде массива (вызвано
// бинарной реализацией листов)
Array findListItemsUtil(TreeNode *treeNode) {
    TreeNode **nodes = malloc(sizeof(TreeNode *) * START_ARRAY_SIZE);
    Array items = {START_ARRAY_SIZE, 0, nodes};

    TreeNode *currentListNode = treeNode;
    do {
        if (currentListNode->childrenNumber == 0) {
            currentListNode = NULL;
        } else if (currentListNode->childrenNumber == 1) {
            addToList(&items, currentListNode->childNodes[0]);
            currentListNode = NULL;
        } else if (currentListNode->childrenNumber == 2) {
            addToList(&items, currentListNode->childNodes[0]);
            currentListNode = currentListNode->childNodes[1];
        } else {
            char exceptionText[1024];
            sprintf(exceptionText,
                    "Exception in list parsing more than two by element id %d",
                    currentListNode->id);
            addException(exceptionText);
            return items;
        }
    } while (currentListNode != NULL);
    return items;
}

// получение корневого элемента из результатов парсинга
TreeNode *findSourceNode(FilenameParseTree input) {
    TreeNode **inputNodes = input.tree->nodes;
    int inputNodesSize = input.tree->size;
    TreeNode *sourceNode = inputNodes[inputNodesSize - 1];
    return sourceNode;
}

// получение списка функций из корневого элемента
Array findSourceItems(TreeNode *source) {
    if (source->childrenNumber != 0) {
        TreeNode *sourceItemsList = source->childNodes[0];
        return findListItemsUtil(sourceItemsList);
    } else {
        return (Array) {0, 0, NULL};
    }
}

CFGNode *initCFGNode(char *text) {
    CFGNode *node = malloc(sizeof(CFGNode));
    node->id = getNextExecutionId();
    node->text = mallocString(text);
    node->definitely = NULL;
    node->conditionally = NULL;
    node->operationTree = NULL;
    node->printed = 0;
    node->listingNode = NULL;
    return node;
}

TreeNode *mallocTreeNode(char *type, char *value, int nodeNumber) {
    TreeNode *node = malloc(sizeof(TreeNode));
    node->id = getNextExecutionId();
    if (type) {
        node->type = mallocString(type);
    } else {
        node->type = NULL;
    }
    if (value) {
        node->value = mallocString(value);
    } else {
        node->value = NULL;
    }
    node->childNodes = malloc(sizeof(TreeNode *) * nodeNumber);
    node->childrenNumber = nodeNumber;
    return node;
}

// создание блока listStatement
CFGNode *executionListStatementNode(TreeNode *treeNode,
                                          CFGNode *nextNode,
                                          CFGNode *breakNode,
                                          FunCalls *funCalls) {
    CFGNode *tmpNextNode = nextNode;
    if (treeNode->childrenNumber == 2) {
        tmpNextNode = executionNode(treeNode->childNodes[1], nextNode, breakNode, funCalls);
    }
    CFGNode *node = initCFGNode("");
    node->definitely =
            executionNode(treeNode->childNodes[0], tmpNextNode, breakNode, funCalls);
    return node;
}

CFGNode *executionVarNode(TreeNode *treeNode, CFGNode *nextNode,
                                CFGNode *breakNode) {
    CFGNode *node = initCFGNode("");
    Array variablesList = {0, 0, NULL};
    TreeNode *typeNode = NULL;
    if (treeNode->childrenNumber == 2) {
        typeNode = treeNode->childNodes[1];
        variablesList = findListItemsUtil(treeNode->childNodes[0]);
    } else {
        typeNode = treeNode->childNodes[0];
    }
    char resultNodeType[1024];
    if (!strcmp(typeNode->type, "array")) {
        int size = 0;
        if (typeNode->childrenNumber == 2) {
            size = findListItemsUtil(typeNode->childNodes[1]).nextPosition;
        }
        sprintf(resultNodeType,
                "array of %s size %d",
                typeNode->value, size);
    } else {
        sprintf(resultNodeType, "%s", typeNode->value);
    }

    CFGNode *previous = node;
    for (int i = 0; i < variablesList.nextPosition; ++i) {
        char varNameAndType[1024];
        sprintf(varNameAndType,
                "AS %s %s",
                ((TreeNode *) variablesList.elements[i])->value, resultNodeType);
        CFGNode *initNode = initCFGNode(varNameAndType);
        TreeNode *initOperationTreeNode = mallocTreeNode("AS", NULL, 2);
        initNode->operationTree = initOperationTreeNode;
        sprintf(varNameAndType,
                "%s",
                resultNodeType);
        initOperationTreeNode->childNodes[0] = mallocTreeNode(NULL, varNameAndType, 0);
        sprintf(varNameAndType,
                "%s",
                ((TreeNode *) variablesList.elements[i])->value);
        initOperationTreeNode->childNodes[1] = mallocTreeNode(NULL, varNameAndType, 0);
        previous->definitely = initNode;
        previous = previous->definitely;
    }
    previous->definitely = nextNode;
    return node;
}

// для построения дерева операций
TreeNode *operationTreeNode(TreeNode *parsingTree, FunCalls *funCalls) {
    TreeNode *node = NULL;

    if (!strcmp(parsingTree->type, "braces")) {
        return operationTreeNode(parsingTree->childNodes[0], funCalls);
    } else if (!strcmp(parsingTree->type, "INCREMENT") || !strcmp(parsingTree->type, "DECREMENT")) {
        node = mallocTreeNode("SET", NULL, 2);
        char valuePlace[1024];
        sprintf(valuePlace,
                "%s",
                parsingTree->childNodes[0]->value);
        node->childNodes[0] = mallocTreeNode(NULL, valuePlace, 0);
        if (!strcmp(parsingTree->type, "INCREMENT")) {
            node->childNodes[1] = mallocTreeNode("PLUS", NULL, 2);
        } else {
            node->childNodes[1] = mallocTreeNode("MINUS", NULL, 2);
        }
        node->childNodes[1]->childNodes[0] = mallocTreeNode(NULL, "const: 1", 0);
        node->childNodes[1]->childNodes[1] = operationTreeNode(parsingTree->childNodes[0], funCalls);
    } else if (!strcmp(parsingTree->type, "callOrIndexer")) {
        if (parsingTree->childrenNumber == 1) {
            node = mallocTreeNode("EXECUTE", NULL, 1);
        } else {
            Array argsArray = findListItemsUtil(parsingTree->childNodes[1]);
            node = mallocTreeNode("EXECUTE", NULL, argsArray.nextPosition + 1);
            for (int i = 0; i < argsArray.nextPosition; ++i) {
                node->childNodes[i + 1] = operationTreeNode(argsArray.elements[i], funCalls);
            }
        }
        char executionName[1024];
        sprintf(executionName, "%s", parsingTree->childNodes[0]->value);
        node->childNodes[0] = mallocTreeNode(NULL, executionName, 0);
        char funCallOperationIdNodeString[1024];
        sprintf(funCallOperationIdNodeString, "%d", node->id);
        TreeNode *funCallOperationIdNode = mallocTreeNode("operationTreeId", funCallOperationIdNodeString, 1);
        TreeNode *calledFunNameNode = mallocTreeNode("call", parsingTree->childNodes[0]->value, 0);
        funCallOperationIdNode->childNodes[0] = calledFunNameNode;
        addToList(funCalls->funCalls, funCallOperationIdNode);
    } else if (parsingTree->childrenNumber == 2) {
        node = mallocTreeNode(parsingTree->type, parsingTree->value, parsingTree->childrenNumber);

        if (!strcmp(parsingTree->type, "SET")) {
            char valuePlace[1024];
            sprintf(valuePlace,
                    "%s",
                    parsingTree->childNodes[0]->value);
            node->childNodes[0] = mallocTreeNode(NULL, valuePlace, 0);
            node->childNodes[1] = operationTreeNode(parsingTree->childNodes[1], funCalls);
        } else {
            node->childNodes[0] = operationTreeNode(parsingTree->childNodes[0], funCalls);
            node->childNodes[1] = operationTreeNode(parsingTree->childNodes[1], funCalls);
        }
    } else if (parsingTree->childrenNumber == 0) {
        if (!strcmp(parsingTree->type, "IDENTIFIER")) {
            node = mallocTreeNode("READ", NULL, 1);
            char valuePlace[1024];
            sprintf(valuePlace,
                    "%s",
                    parsingTree->value);
            node->childNodes[0] = mallocTreeNode(NULL, valuePlace, 0);
        } else {
            node = mallocTreeNode("CONST", NULL, 2);
            char *typeByLiteral = "";
            if (!strcmp(parsingTree->type, "DEC")) {
                typeByLiteral = "int";
            } else if (!strcmp(parsingTree->type, "BIN")) {
                typeByLiteral = "int";
            } else if (!strcmp(parsingTree->type, "HEX")) {
                typeByLiteral = "int";
            } else if (!strcmp(parsingTree->type, "CHAR")) {
                typeByLiteral = "char";
            } else if (!strcmp(parsingTree->type, "STR")) {
                typeByLiteral = "str";
            } else if (!strcmp(parsingTree->type, "BOOL")) {
                typeByLiteral = "bool";
            }
            char constVal[1024];
            sprintf(constVal, "%s", typeByLiteral);
            node->childNodes[0] = mallocTreeNode(NULL, typeByLiteral, 0);
            node->childNodes[1] = mallocTreeNode(NULL, parsingTree->value, 0);
        }
    }
    return node;
}

char *expressionNodeToString(TreeNode *treeNode) {
    if (treeNode->childrenNumber == 0) {
        return mallocString(treeNode->value);
    } else if (treeNode->childrenNumber == 1) {
        char *childStr = expressionNodeToString(treeNode->childNodes[0]);
        char text[1024];
        sprintf(text,
                "%s-%s-",
                treeNode->type, childStr);
        return mallocString(text);
    } else {
        char *childLeftStr = expressionNodeToString(treeNode->childNodes[0]);
        char *childRightStr = expressionNodeToString(treeNode->childNodes[1]);
        char text[1024];
        sprintf(text,
                "%s %s %s",
                childLeftStr, treeNode->type, childRightStr);
        return mallocString(text);
    }
}

CFGNode *executionExpressionNode(TreeNode *treeNode, FunCalls *funCalls) {
    CFGNode *node = initCFGNode(expressionNodeToString(treeNode));
    node->operationTree = operationTreeNode(treeNode, funCalls);
    return node;
}

CFGNode *executionElseNode(TreeNode *treeNode, CFGNode *nextNode,
                                 CFGNode *breakNode, FunCalls *funCalls) {
    CFGNode *node = initCFGNode("");
    if (treeNode->childrenNumber == 1) {
        node->definitely = executionNode(treeNode->childNodes[0],
                                         nextNode, breakNode, funCalls);
    } else {
        node->definitely = nextNode;
    }
    return node;
}

CFGNode *executionIfNode(TreeNode *treeNode, CFGNode *nextNode,
                               CFGNode *breakNode, FunCalls *funCalls) {
    CFGNode *node = initCFGNode("");
    TreeNode *elseTreeNode = NULL;
    TreeNode *ifStatements = NULL;
    if (treeNode->childrenNumber == 3) {
        //    случай когда есть стейтменты в if и существует else
        ifStatements = treeNode->childNodes[1];
        elseTreeNode = treeNode->childNodes[2];
    } else if (treeNode->childrenNumber == 2 &&
               !strcmp(treeNode->childNodes[1]->type, "else")) {
        //    когда нет стейтментов в if
        elseTreeNode = treeNode->childNodes[1];
    } else if (treeNode->childrenNumber == 2) {
        ifStatements = treeNode->childNodes[1];
    }

    CFGNode *conditionNextNode = NULL;
    CFGNode *conditionConditionallyNode = NULL;
    if (elseTreeNode != NULL) {
        CFGNode *elseNode =
                executionElseNode(elseTreeNode, nextNode, breakNode, funCalls);
        conditionNextNode = elseNode;
    } else {
        conditionNextNode = nextNode;
    }
    if (ifStatements != NULL) {
        CFGNode *statementsNode =
                executionNode(ifStatements, nextNode, breakNode, funCalls);
        conditionConditionallyNode = statementsNode;
    }
    CFGNode *conditionNode = executionExpressionNode(treeNode->childNodes[0], funCalls);
    node->definitely = conditionNode;
    conditionNode->definitely = conditionNextNode;
    conditionNode->conditionally = conditionConditionallyNode;
    return node;
}

CFGNode *executionWhileNode(TreeNode *treeNode, CFGNode *nextNode,
                                  CFGNode *breakNode, FunCalls *funCalls) {
    CFGNode *node = initCFGNode("");
    CFGNode *statementNode = NULL;
    if (treeNode->childrenNumber == 2) {
        statementNode = executionNode(treeNode->childNodes[1], node, nextNode, funCalls);
    } else {
        statementNode = initCFGNode("");
        statementNode->definitely = node;
    }
    CFGNode *conditionNode = executionExpressionNode(treeNode->childNodes[0], funCalls);
    node->definitely = conditionNode;
    conditionNode->definitely = nextNode;
    conditionNode->conditionally = statementNode;
    return node;
}

CFGNode *executionDoNode(TreeNode *treeNode, CFGNode *nextNode,
                               CFGNode *breakNode, FunCalls *funCalls) {
    CFGNode *node = initCFGNode("");
    TreeNode *conditionTreeNode = NULL;
    if (treeNode->childrenNumber == 3) {
        conditionTreeNode = treeNode->childNodes[2];
    } else {
        conditionTreeNode = treeNode->childNodes[1];
    }
    CFGNode *doConditionNode = executionExpressionNode(conditionTreeNode, funCalls);
    doConditionNode->definitely = nextNode;
    doConditionNode->conditionally = node;
    CFGNode *statementNode = NULL;
    if (treeNode->childrenNumber == 3) {
        statementNode = executionNode(treeNode->childNodes[0], doConditionNode, nextNode, funCalls);
    } else {
        statementNode = initCFGNode("");
        statementNode->definitely = doConditionNode;
    }
    node->definitely = statementNode;
    return node;
}

CFGNode *executionBreakNode(TreeNode *treeNode, CFGNode *nextNode,
                                  CFGNode *breakNode) {
    CFGNode *node = initCFGNode("");
    if (breakNode == NULL) {
        char exceptionText[1024];
        sprintf(exceptionText,
                "Exception in BREAK tree node parsing id --> %d no loop for break found",
                treeNode[0].id);
        addException(exceptionText);
        CFGNode *exceptionNode = initCFGNode(exceptionText);
        node->definitely = exceptionNode;
        exceptionNode->definitely = nextNode;
    } else {
        node->definitely = breakNode;
    }
    return node;
}

CFGNode *executionNode(TreeNode *treeNode, CFGNode *nextNode,
                             CFGNode *breakNode, FunCalls *funCalls) {
    if (!strcmp(treeNode[0].type, "listStatement")) {
        return executionListStatementNode(treeNode, nextNode, breakNode, funCalls);
    } else if (!strcmp(treeNode[0].type, "var")) {
        return executionVarNode(treeNode, nextNode, breakNode);
    } else if (!strcmp(treeNode[0].type, "if")) {
        return executionIfNode(treeNode, nextNode, breakNode, funCalls);
    } else if (!strcmp(treeNode[0].type, "while")) {
        return executionWhileNode(treeNode, nextNode, breakNode, funCalls);
    } else if (!strcmp(treeNode[0].type, "break")) {
        return executionBreakNode(treeNode, nextNode, breakNode);
    } else if (!strcmp(treeNode[0].type, "do")) {
        return executionDoNode(treeNode, nextNode, breakNode, funCalls);
    } else {
        CFGNode *expressionNode = executionExpressionNode(treeNode, funCalls);
        expressionNode->definitely = nextNode;
    }
}

CFGNode *functionArgsCFGNode(TreeNode *functionSignatureNode, CFGNode *nextNode,
                                         CFGNode *breakNode, FunCalls *funCalls) {
    CFGNode *node = initCFGNode("");
    node->definitely = nextNode;
    if (functionSignatureNode->childrenNumber > 0 &&
        !strcmp(functionSignatureNode->childNodes[0]->type, "listArgDef")) {
        Array args = findListItemsUtil(functionSignatureNode->childNodes[0]);
        CFGNode *parentNode = node;
        for (int i = 0; i < args.nextPosition; ++i) {
            TreeNode *argDef = args.elements[i];
            char argText[1024];
            sprintf(argText, "ARG %s %s", argDef->childNodes[0]->value, argDef->childNodes[1]->value);
            CFGNode *newDimNode = initCFGNode(argText);
            newDimNode->operationTree = mallocTreeNode("ARG", NULL, 2);
            newDimNode->operationTree->childNodes[0] = mallocTreeNode(NULL, argDef->childNodes[1]->value, 0);
            newDimNode->operationTree->childNodes[1] = mallocTreeNode(NULL, argDef->childNodes[0]->value, 0);
            newDimNode->definitely = parentNode->definitely;
            parentNode->definitely = newDimNode;
            parentNode = newDimNode;
        }
        parentNode->definitely = nextNode;
    }
    return node;
}

CFGNode *initGraph(TreeNode *sourceItem, FunCalls *funCalls) {
    CFGNode *startNode = initCFGNode("START");
    CFGNode *endNode = initCFGNode("FINISH");
    TreeNode *funcDef = sourceItem->childNodes[0];

    CFGNode *listStatements = endNode;
    if (funcDef->childrenNumber == 2) {
        listStatements =
                executionNode(funcDef->childNodes[1], endNode, NULL, funCalls);
    }
    CFGNode *functionArgs =
            functionArgsCFGNode(funcDef->childNodes[0], listStatements, NULL, funCalls);
    startNode->definitely = functionArgs;
    return startNode;
}

void initExceptions() {
    void **nodes = malloc(sizeof(char *) * START_ARRAY_SIZE);
    exceptions = (Array) {START_ARRAY_SIZE, 0, nodes};
}

Array *executionGraph(FilenameParseTree *input, int size) {
    void **resultNodes = malloc(sizeof(FunExecution *) * START_ARRAY_SIZE);
    Array *result = malloc(sizeof(Array));
    result->size = START_ARRAY_SIZE;
    result->nextPosition = 0;
    result->elements = resultNodes;

    for (int i = 0; i < size; ++i) {
        initExceptions();
        TreeNode *sourceNode = findSourceNode(input[i]);
        Array sourceItems = findSourceItems(sourceNode);
        for (int j = 0; j < sourceItems.nextPosition; ++j) {
            FunExecution *currentFunExecution = malloc(sizeof(FunExecution));
            currentFunExecution->filename = input[i].filename;
            TreeNode **currentSourceItemElements =
                    ((TreeNode **) sourceItems.elements);
            currentFunExecution->name =
                    currentSourceItemElements[j]->childNodes[0]->childNodes[0]->value;
            currentFunExecution->signature =
                    currentSourceItemElements[j]->childNodes[0];

            void **nodes = malloc(sizeof(TreeNode *) * START_ARRAY_SIZE);
            Array funs = (Array) {START_ARRAY_SIZE, 0, nodes};
            FunCalls funCalls = (FunCalls) {&funs, currentFunExecution->name};
            currentFunExecution->nodes = initGraph(currentSourceItemElements[j], &funCalls);
            TreeNode *funCallsRoot = mallocTreeNode("currentFunction", currentFunExecution->name,
                                                    funCalls.funCalls->nextPosition);
            for (int k = 0; k < funCalls.funCalls->nextPosition; ++k) {
                funCallsRoot->childNodes[k] = funCalls.funCalls->elements[k];
            }
            currentFunExecution->funCalls = funCallsRoot;
            currentFunExecution->errorsCount = exceptions.nextPosition;
            currentFunExecution->errors = exceptions.elements;
            addToList(result, currentFunExecution);
        }
    }
    return result;
}

void printNode(TreeNode *node, FILE *outputFile) {
    if (node == NULL) {
        return;
    }
    int childrenNumber = node->childrenNumber;
    for (int i = 0; i < childrenNumber; ++i) {
        printNode(node->childNodes[i], outputFile);
        fprintf(outputFile, "node%d", node->id);
        fprintf(outputFile, "([");
        int typeExists = 0;
        if (node->type != NULL && strlen(node->type) > 0) {
            fprintf(outputFile, "Type: %s", node->type);
            typeExists = 1;
        }
        if (node->value != NULL && strlen(node->value) > 0) {
            if (typeExists) {
                fprintf(outputFile, ", ", node->value);
            }
            fprintf(outputFile, "Value: %s", node->value);
        }
        fprintf(outputFile, "])");

        TreeNode *childNode = node->childNodes[i];
        fprintf(outputFile, " --> ");
        fprintf(outputFile, "node%d", childNode->id);
        fprintf(outputFile, "([");
        typeExists = 0;
        if (childNode->type != NULL && strlen(childNode->type) > 0) {
            fprintf(outputFile, "Type: %s", childNode->type);
            typeExists = 1;
        }
        if (childNode->value != NULL && strlen(childNode->value) > 0) {
            if (typeExists) {
                fprintf(outputFile, ", ");
            }
            fprintf(outputFile, "Value: %s", childNode->value);
        }
        fprintf(outputFile, "])");
        fprintf(outputFile, "\n");
    }
}

void printTreeNode(TreeNode *node, FILE *outputFile) {
    fprintf(outputFile, "flowchart TB\n");
    printNode(node, outputFile);
    fprintf(outputFile, "\n");
}

void printCFGNode(CFGNode *father, CFGNode *child, FILE *outputFile, char *relationName) {
    fprintf(outputFile, "node%d", father->id);
    fprintf(outputFile, "([");
    fprintf(outputFile, "Text: %s", father->text);
    fprintf(outputFile, "])");
    fprintf(outputFile, " --%s--> ", relationName);
    fprintf(outputFile, "node%d", child->id);
    fprintf(outputFile, "([");
    fprintf(outputFile, "Text: %s", child->text);
    fprintf(outputFile, "])");
    fprintf(outputFile, "\n");
}

void printExecutionGraphNodeToFile(CFGNode *executionNode, FILE *outputOperationTreesFile,
                                   FILE *outputExecutionFile) {
    if (executionNode->printed) {
        return;
    } else {
        executionNode->printed = 1;
    }

    if (executionNode->operationTree) {
        char linkedCFGNodeId[1024];
        sprintf(linkedCFGNodeId, "%d", executionNode->id);
        TreeNode *linkedCFGNode = mallocTreeNode("linked execution node id", linkedCFGNodeId, 1);
        linkedCFGNode->childNodes[0] = executionNode->operationTree;
        printNode(linkedCFGNode, outputOperationTreesFile);
    }

    CFGNode *definitely = executionNode->definitely;
    if (definitely) {
        printExecutionGraphNodeToFile(definitely, outputOperationTreesFile, outputExecutionFile);

        printCFGNode(executionNode, definitely, outputExecutionFile, "definitely");
    }

    CFGNode *conditionally = executionNode->conditionally;
    if (conditionally) {
        printExecutionGraphNodeToFile(conditionally, outputOperationTreesFile, outputExecutionFile);

        printCFGNode(executionNode, conditionally, outputExecutionFile, "conditionally");
    }
}

void printExecutionGraphToFile(CFGNode *executionNode, FILE *outputOperationTreesFile,
                               FILE *outputExecutionFile) {
    fprintf(outputOperationTreesFile, "flowchart TB\n");
    fprintf(outputExecutionFile, "flowchart TB\n");
    printExecutionGraphNodeToFile(executionNode, outputOperationTreesFile, outputExecutionFile);
    fprintf(outputOperationTreesFile, "\n");
    fprintf(outputExecutionFile, "\n");
}

void printExecution(FunExecution *funExecution, FILE *outputFunCallFile, FILE *outputOperationTreesFile,
                    FILE *outputExecutionFile) {
    printTreeNode(funExecution->funCalls, outputFunCallFile);
    printExecutionGraphToFile(funExecution->nodes, outputOperationTreesFile, outputExecutionFile);
}
