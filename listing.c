#include "listing.h"
#include "cfg.h"

void initListingParseNode(CFGNode *CFGNode) {
    ListingNode *node = malloc(sizeof(ListingNode));
    node->node = CFGNode;
    node->label = NULL;
    node->checked = 1;
    CFGNode->listingNode = node;
}

void tryPlaceLabel(CFGNode *CFGNode, int *labelCounter, bool necessaryLabeled) {
    if (CFGNode == NULL) {
        return;
    }
    bool existListing = false;
    if (CFGNode->listingNode != NULL) {
        existListing = true;
    } else {
        initListingParseNode(CFGNode);
    }
    if (existListing || necessaryLabeled) {
        char *existingLabel = CFGNode->listingNode->label;
        if (existingLabel == NULL) {
            ++(*labelCounter);
            char *label = malloc(sizeof(char) * 10);
            sprintf(label, "label%d", *labelCounter);
            CFGNode->listingNode->label = label;
        }
    }
    if (!existListing) {
        tryPlaceLabel(CFGNode->definitely, labelCounter, false);
        tryPlaceLabel(CFGNode->conditionally, labelCounter, true);
    }
}

void placeLabels(Array *funExecutions) {
    int labelCounter = 0;
    for (int i = 0; i < funExecutions->nextPosition; ++i) {
        FunExecution *funExecution = funExecutions->elements[i];
        tryPlaceLabel(funExecution->nodes, &labelCounter, false);
    }
}

ValuePlaceAssociation *findValuePlace(Array *valuePlaceAssociations, char *name) {
    for (int i = 0; i < valuePlaceAssociations->nextPosition; ++i) {
        ValuePlaceAssociation *valuePlaceAssociation = valuePlaceAssociations->elements[i];
        if (!strcmp(valuePlaceAssociation->name, name)) {
            return valuePlaceAssociation;
        }
    }
    return NULL;
}

void printException(char *message) {
    printf("EXCEPTION: %s", message);
}

void fprintlnWithArg(char *message, char *arg, FILE *file) {
    fprintf(file, "%s %s\n", message, arg);
}

void fprintln(char *message, FILE *file) {
    fprintf(file, "%s\n", message);
}

ValuePlaceAssociation *addArgumentPlace(Array *valuePlaceAssociations, char *argName, char *argType) {
    ValuePlaceAssociation *findRes = findValuePlace(valuePlaceAssociations, argName);
    if (findRes != NULL) {
        char exceptionMessage[1000];
        sprintf(exceptionMessage, "duplicate argument name %s", findRes->name);
        printException(exceptionMessage);
        return findRes;
    } else {
        for (int i = 0; i < valuePlaceAssociations->nextPosition; ++i) {
            ValuePlaceAssociation *previousArg = valuePlaceAssociations->elements[i];
            previousArg->shiftPosition -= 2;
        }
        ValuePlaceAssociation *newArgAssociation = malloc(sizeof(ValuePlaceAssociation));
        newArgAssociation->name = argName;
        newArgAssociation->type = argType;
        newArgAssociation->shiftPosition = -2;
        addToList(valuePlaceAssociations, newArgAssociation);
        return newArgAssociation;
    }
}

ValuePlaceAssociation *addValuePlace(Array *valuePlaceAssociations, char *valuePlaceName, char *valuePlaceType) {
    ValuePlaceAssociation *findRes = findValuePlace(valuePlaceAssociations, valuePlaceName);
    if (findRes != NULL) {
        char exceptionMessage[1000];
        sprintf(exceptionMessage, "duplicate value place name %s", findRes->name);
        printException(exceptionMessage);
        return findRes;
    } else {
        ValuePlaceAssociation *newAssociation = malloc(sizeof(ValuePlaceAssociation));
        newAssociation->name = valuePlaceName;
        newAssociation->type = valuePlaceType;
        ValuePlaceAssociation *lastElement = NULL;
        if (valuePlaceAssociations->nextPosition > 0) {
            lastElement = valuePlaceAssociations->elements[valuePlaceAssociations->nextPosition - 1];
        }
        // 0 - адрес возврата, 1-2 возвращаемое значение
        int newElementShift = 3;
        if (lastElement != NULL && lastElement->shiftPosition > 0) {
            newElementShift = lastElement->shiftPosition + 2;
        }
        newAssociation->shiftPosition = newElementShift;
        addToList(valuePlaceAssociations, newAssociation);
        return newAssociation;
    }
}

void tryPrintOperationTreeNode(TreeNode *operationTree, FILE *listingFile, Array *valuePlaceAssociations,
                               int *argumentNumber) {
    char *operationType = operationTree->type;
    if (!strcmp(operationType, "ARG")) {
        (*argumentNumber)++;
        addArgumentPlace(
                valuePlaceAssociations,
                operationTree->childNodes[1]->value,
                operationTree->childNodes[0]->value
        );
    } else if (!strcmp(operationType, "AS")) {
        addValuePlace(
                valuePlaceAssociations,
                operationTree->childNodes[1]->value,
                operationTree->childNodes[0]->value
        );
        fprintln("PUSH 0", listingFile);
        fprintln("PUSH 0", listingFile);
    } else if (!strcmp(operationType, "CONST")) {
        if (!strcmp(operationTree->childNodes[0]->value, "int")) {
            fprintlnWithArg("PUSH", "1", listingFile);
            fprintlnWithArg("PUSH", operationTree->childNodes[1]->value, listingFile);
        } else if (!strcmp(operationTree->childNodes[0]->value, "char")) {
            char value[1];
            sprintf(value, "%d", (int) operationTree->childNodes[1]->value[0]);
            fprintlnWithArg("PUSH", "3", listingFile);
            fprintlnWithArg("PUSH", value, listingFile);
        } else if (!strcmp(operationTree->childNodes[0]->value, "bool")) {
            if (!strcmp(operationTree->childNodes[1]->value, "true")) {
                fprintlnWithArg("PUSH", "1", listingFile);
                fprintlnWithArg("PUSH", "1", listingFile);
            } else {
                fprintlnWithArg("PUSH", "1", listingFile);
                fprintlnWithArg("PUSH", "0", listingFile);
            }
        } else {
            fprintln("EXCEPTION", listingFile);
        }
    } else if (!strcmp(operationType, "SET")) {
        tryPrintOperationTreeNode(operationTree->childNodes[1], listingFile, valuePlaceAssociations, argumentNumber);
        ValuePlaceAssociation *valuePlace = findValuePlace(valuePlaceAssociations, operationTree->childNodes[0]->value);
        if (valuePlace == NULL) {
            char exceptionMessage[1000];
            sprintf(exceptionMessage, "value place not found by name %s", operationTree->childNodes[0]->value);
            printException(exceptionMessage);
            return;
        }
        char valuePlaceShift[1000];
        sprintf(valuePlaceShift, "%d", valuePlace->shiftPosition);
        fprintlnWithArg("SAVE_BP", valuePlaceShift, listingFile);
    } else if (!strcmp(operationType, "READ")) {
        ValuePlaceAssociation *valuePlace = findValuePlace(valuePlaceAssociations, operationTree->childNodes[0]->value);
        if (valuePlace == NULL) {
            char exceptionMessage[1000];
            sprintf(exceptionMessage, "value place not found by name %s", operationTree->childNodes[0]->value);
            printException(exceptionMessage);
            return;
        }
        char valuePlaceShift[1000];
        sprintf(valuePlaceShift, "%d", valuePlace->shiftPosition);
        fprintlnWithArg("LOAD_BP", valuePlaceShift, listingFile);
    } else if (!strcmp(operationType, "EQUALITY")) {
        tryPrintOperationTreeNode(operationTree->childNodes[0], listingFile, valuePlaceAssociations, argumentNumber);
        tryPrintOperationTreeNode(operationTree->childNodes[1], listingFile, valuePlaceAssociations, argumentNumber);
        fprintln("EQ", listingFile);
    } else if (!strcmp(operationType, "LESSTHANEQ")) {
        tryPrintOperationTreeNode(operationTree->childNodes[0], listingFile, valuePlaceAssociations, argumentNumber);
        tryPrintOperationTreeNode(operationTree->childNodes[1], listingFile, valuePlaceAssociations, argumentNumber);
        fprintln("LTE", listingFile);
    } else if (!strcmp(operationType, "GREATERTHANEQ")) {
        tryPrintOperationTreeNode(operationTree->childNodes[0], listingFile, valuePlaceAssociations, argumentNumber);
        tryPrintOperationTreeNode(operationTree->childNodes[1], listingFile, valuePlaceAssociations, argumentNumber);
        fprintln("GTE", listingFile);
    } else if (!strcmp(operationType, "LESSTHAN")) {
        tryPrintOperationTreeNode(operationTree->childNodes[0], listingFile, valuePlaceAssociations, argumentNumber);
        tryPrintOperationTreeNode(operationTree->childNodes[1], listingFile, valuePlaceAssociations, argumentNumber);
        fprintln("LT", listingFile);
    } else if (!strcmp(operationType, "GREATERTHAN")) {
        tryPrintOperationTreeNode(operationTree->childNodes[0], listingFile, valuePlaceAssociations, argumentNumber);
        tryPrintOperationTreeNode(operationTree->childNodes[1], listingFile, valuePlaceAssociations, argumentNumber);
        fprintln("GT", listingFile);
    } else if (!strcmp(operationType, "NOTEQUAL")) {
        tryPrintOperationTreeNode(operationTree->childNodes[0], listingFile, valuePlaceAssociations, argumentNumber);
        tryPrintOperationTreeNode(operationTree->childNodes[1], listingFile, valuePlaceAssociations, argumentNumber);
        fprintln("NEQ", listingFile);
    } else if (!strcmp(operationType, "SUM")) {
        tryPrintOperationTreeNode(operationTree->childNodes[0], listingFile, valuePlaceAssociations, argumentNumber);
        tryPrintOperationTreeNode(operationTree->childNodes[1], listingFile, valuePlaceAssociations, argumentNumber);
        fprintln("SUM", listingFile);
    } else if (!strcmp(operationType, "SUB")) {
        tryPrintOperationTreeNode(operationTree->childNodes[0], listingFile, valuePlaceAssociations, argumentNumber);
        tryPrintOperationTreeNode(operationTree->childNodes[1], listingFile, valuePlaceAssociations, argumentNumber);
        fprintln("SUB", listingFile);
    } else if (!strcmp(operationType, "MUL")) {
        tryPrintOperationTreeNode(operationTree->childNodes[0], listingFile, valuePlaceAssociations, argumentNumber);
        tryPrintOperationTreeNode(operationTree->childNodes[1], listingFile, valuePlaceAssociations, argumentNumber);
        fprintln("MUL", listingFile);
    } else if (!strcmp(operationType, "DIV")) {
        tryPrintOperationTreeNode(operationTree->childNodes[0], listingFile, valuePlaceAssociations, argumentNumber);
        tryPrintOperationTreeNode(operationTree->childNodes[1], listingFile, valuePlaceAssociations, argumentNumber);
        fprintln("DIV", listingFile);
    } else if (!strcmp(operationType, "PERCENT")) {
        tryPrintOperationTreeNode(operationTree->childNodes[0], listingFile, valuePlaceAssociations, argumentNumber);
        tryPrintOperationTreeNode(operationTree->childNodes[1], listingFile, valuePlaceAssociations, argumentNumber);
        fprintln("MOD", listingFile);
    } else if (!strcmp(operationType, "EXECUTE")) {
        // if (!strcmp(operationTree->childNodes[0]->value, "input")) {
        //     fprintln("LOAD_IN", listingFile);
        // } else if (!strcmp(operationTree->childNodes[0]->value, "output")) {
        //     tryPrintOperationTreeNode(
        //             operationTree->childNodes[1],
        //             listingFile,
        //             valuePlaceAssociations,
        //             argumentNumber
        //     );
        //     fprintln("SAVE_OUT", listingFile);
        // } else {
        for (int i = 1; i < operationTree->childrenNumber; ++i) {
            tryPrintOperationTreeNode(operationTree->childNodes[i], listingFile, valuePlaceAssociations,
                                        argumentNumber);
        }
        fprintlnWithArg("CALL", operationTree->childNodes[0]->value, listingFile);
        // }
    } else {
        fprintln("EXCEPTION", listingFile);
    }
}

void tryPrintNode(CFGNode *CFGNode, FILE *listingFile, Array *valuePlaceAssociations, int *argumentNumber) {
    if (CFGNode == NULL) {
        return;
    }
    if (CFGNode->listingNode->label != NULL) {
        char label[1000];
        sprintf(label, "%s:", CFGNode->listingNode->label);
        fprintln(label, listingFile);
    }
    CFGNode->listingNode->checked++;
    if (CFGNode->operationTree != NULL) {
        tryPrintOperationTreeNode(CFGNode->operationTree, listingFile, valuePlaceAssociations, argumentNumber);
        if (CFGNode->conditionally != NULL) {
            fprintlnWithArg("JNZ", CFGNode->conditionally->listingNode->label, listingFile);
        }
    }
    if (CFGNode->definitely == NULL) {
        char argNumberString[10];
        sprintf(argNumberString, "%d", *argumentNumber);
        fprintln("SAVE_BP 1", listingFile);
        fprintlnWithArg("RET", argNumberString, listingFile);
        return;
    }
    if (CFGNode->definitely->listingNode->checked > 1) {
        fprintlnWithArg("JMP", CFGNode->definitely->listingNode->label, listingFile);
    } else {
        tryPrintNode(CFGNode->definitely, listingFile, valuePlaceAssociations, argumentNumber);
    }
    if (CFGNode->conditionally != NULL && CFGNode->conditionally->listingNode->checked > 1) {
        return;
    } else {
        tryPrintNode(CFGNode->conditionally, listingFile, valuePlaceAssociations, argumentNumber);
    }
}

void printListing(Array *funExecutions, FILE *listingFile) {
    fprintln("[section code]", listingFile);
    fprintln("CALL main", listingFile);
    fprintln("POP", listingFile);
    fprintln("HLT", listingFile);
    for (int i = 0; i < funExecutions->nextPosition; ++i) {
        int argumentNumber = 0;
        Array *valuePlaceAssociationsArray = malloc(sizeof(Array));
        valuePlaceAssociationsArray->size = 100;
        valuePlaceAssociationsArray->nextPosition = 0;
        valuePlaceAssociationsArray->elements = malloc(sizeof(ValuePlaceAssociation) * 100);
        FunExecution *funExecution = funExecutions->elements[i];
        char funLabel[1000];
        sprintf(funLabel, "%s:", funExecution->name);
        fprintln(funLabel, listingFile);
        fprintln("PUSH 0", listingFile);
        fprintln("PUSH 0", listingFile);
        tryPrintNode(funExecution->nodes, listingFile, valuePlaceAssociationsArray, &argumentNumber);
        free(valuePlaceAssociationsArray);
    }
}