#ifndef SYSSOFT_LISTING_H
#define SYSSOFT_LISTING_H

#include "main.h"

typedef struct ListingNode ListingNode;
typedef struct ValuePlaceAssociation ValuePlaceAssociation;

struct ListingNode {
    CFGNode *node;
    char *label;
    int checked;
};

struct ValuePlaceAssociation {
    char *name;
    char *type;
    int shiftPosition;
};

void placeLabels(Array *funExecutions);

void printListing(Array *funExecutions, FILE *listingFile);

#endif //SYSSOFT_LISTING_H
