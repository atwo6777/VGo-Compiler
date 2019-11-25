#include "tree.h"
#include "semantic.h"
#include "symboltable.h"
#include "vgobison.tab.h"
#include "nonterminal.h"
#include "semantic.h"
#include "globalutilities.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "linkedlist.h"
#include "location.h"

struct symboltable *globalSymbolTable;
struct symboltable *currentSymbolTable;

struct symboltable *fmtSymbolTable;
struct symboltable *timeSymbolTable;
struct symboltable *mathSymbolTable;

void scopeAnalysis(struct Node *treeHead);
void checkChildren(struct Node *treeHead);
void printChildren(struct Node *treeHead);
void handlePackage(struct Node *treeHead);
void handleImportPackage(struct Node *treeHead);
void handleStruct(struct Node *treeHead);
void handleFunctionDeclaration(struct Node *treeHead);
void handleVariableDeclaration(struct Node *treeHead);
void handlePotentialStructInstance(struct Node *treeHead);
void lookForVariableNames(struct Node *treeHead, int type, char *typeName);
void lookForParameterNames(struct Node *treeHead);
void lookForReturnTypes(struct Node *treeHead);
void handleVariableInstance(struct Node *treeHead);
void lookForStructVariables(struct Node *treeHead, struct symboltable *currentStruct);
void findConstName(struct Node *treeHead, int type, char *typeName);
void handleConst(struct Node *treeHead);
int typeAnalysis(struct Node *treeHead);
int checkTypeChildren(struct Node *treeHead);
int findTerminal(struct Node *treeHead);
char *getTerminalText(struct Node *treeHead);
void checkTypeFunctionDeclaration(struct Node *treeHead);
int checkTypeFunctionCall(struct Node *treeHead);
void checkTypeNonDclStmt(struct Node *treeHead);
int checkTypeExpression(struct Node *treeHead);
int checkTypeSimpleStatement(struct Node *treeHead);
int checkTypepexpr_no_paren(struct Node *treeHead);
int checkTypeDefault(struct Node *treeHead);
void checkForHeader(struct Node *treeHead);
void addLabelsToFunctionSymbolTable(struct symboltable *currentFunctionSymbolTable);
void addLabelsToFunctionLinkedList(struct LinkedListNode *head);
void addLabelsToFunction(struct Node *treeHead);
void addLabelsToLinkedList(struct LinkedListNode *head);
void checkLabelChildren(struct Node *treeHead, int region, int *regionOffset);
void generateLabels(struct Node *treeHead, int region, int *regionOffset);
void handleIfLocation(struct Node *treeHead, int region, int *regionOffset);
void handleForLocation(struct Node *treeHead, int region, int *regionOffset);
struct operation *beginIntermediateCodeGeneration(struct Node *treeHead);
struct operation *checkCodeGenerationChildren(struct Node *treeHead);
struct operation *generateFunctionOperation(struct Node *treeHead);
struct operation *generateExpressionOperation(struct Node *treeHead);
struct operation *generateSimpleStmtOperation(struct Node *treeHead);
struct operation *callFunction(struct Node *treeHead);

struct LinkedListNode *checkParameterTypes(struct symboltable *functionSymbolTable, struct LinkedListNode *listHead, struct Node *treeHead);
int arraySize = -1;

int offsetGLOBAL = 0;
int offsetLOCAL = 0;
int offsetFUNCTION = 0;
int offsetCONST = 0;
int offsetSTRING = 0;

void beginSemanticAnalysis(struct Node *treeHead)
{
    globalSymbolTable = createSymbolTable("Global Scope", NULL);
    currentSymbolTable = globalSymbolTable;
    scopeAnalysis(treeHead);
    if (printCode == 3)
    {
        printSymbolTable(globalSymbolTable);
        printFunctionSymbolTable();
        printStructSymbolTable();
    }
    typeAnalysis(treeHead);

    generateLabels(treeHead, REGIONGLOBAL, &offsetGLOBAL);

    struct operation *programHead = beginIntermediateCodeGeneration(treeHead);
    printf("Finished Execution\n");
    printOperationLinkedList(programHead, currentfile);
}

void scopeAnalysis(struct Node *treeHead)
{
    if (treeHead != NULL)
    {
        switch (treeHead->category)
        {
        case package:
            handlePackage(treeHead);
            break;

        case import_here:
            handleImportPackage(treeHead);
            break;

        case typedcl:
            handleStruct(treeHead);
            break;

        case xfndcl:
            handleFunctionDeclaration(treeHead);
            break;

        case constdcl:
            handleConst(treeHead);
            break;

        case vardcl:
            handleVariableDeclaration(treeHead);
            break;

        case LNAME:
            handleVariableInstance(treeHead);
            break;

        default:
            checkChildren(treeHead);
            break;
        }
    }
}

int typeAnalysis(struct Node *treeHead)
{

    if (treeHead != NULL)
    {
        switch (treeHead->category)
        {
        case xfndcl:
            checkTypeFunctionDeclaration(treeHead);
            break;

        case pseudocall:
            return checkTypeFunctionCall(treeHead);
            break;

        case non_dcl_stmt:
            checkTypeNonDclStmt(treeHead);
            break;

        case expr:
            return checkTypeExpression(treeHead);
            break;

        case simple_stmt:
            return checkTypeSimpleStatement(treeHead);
            break;

        case pexpr_no_paren:
            checkTypepexpr_no_paren(treeHead);
            break;

        case for_header:
            checkForHeader(treeHead);
            break;

        default:
            return checkTypeDefault(treeHead);
            break;
        }
    }
    // backup return in case its an issue
    return -1;
}

struct LinkedListNode *checkParameterTypes(struct symboltable *functionSymbolTable, struct LinkedListNode *listHead, struct Node *treeHead)
{

    if (treeHead == NULL)
    {
        // do nothing
    }
    else if (treeHead->numberOfChildren > 0)
    {
        int i = 0;
        for (i = 0; i < treeHead->numberOfChildren; i++)
        {

            listHead = checkParameterTypes(functionSymbolTable, listHead, treeHead->children[i]);
        }
    }
    else if (treeHead->data->category == COMA)
    {
        // do nothing we have a ,
    }
    else
    {
        struct Symbol *newData = calloc(1, sizeof(struct Symbol));
        if (treeHead->data->category == NUMERICLITERAL)
        {
            newData->type = INT;
        }
        else
        {

            newData->type = treeHead->data->category;
        }
        newData->name = treeHead->data->text;
        newData->typeName = findTypeName(newData->type);
        newData->arraySize = -1;
        newData->isConst = 0;
        listHead = addToEnd(newData, listHead);
    }
    return listHead;
}

int checkTypeChildren(struct Node *treeHead)
{
    int i = 0;
    if (treeHead != NULL)
    {
        if (treeHead->numberOfChildren == 1)
        {
            return typeAnalysis(treeHead->children[0]);
        }
        else
        {
            for (i = 0; i < treeHead->numberOfChildren; i++)
            {
                int currentType = typeAnalysis(treeHead->children[i]);
                if (currentType > 0)
                {
                }
            }
        }
    }
    return -1;
}

int findTerminal(struct Node *treeHead)
{
    if (treeHead == NULL)
    {
        return -1;
    }
    else if (treeHead->numberOfChildren > 0)
    {
        return findTerminal(treeHead->children[0]);
    }
    else if (treeHead->data->category == LNAME)
    {
        return findTypeInSymbolTable(currentSymbolTable, treeHead->data->text);
    }
    else
    {
        if (treeHead->data->category == NUMERICLITERAL)
        {
            return INT;
        }
        return treeHead->data->category;
    }
}

void checkChildren(struct Node *treeHead)
{
    if (treeHead->numberOfChildren > 0)
    {
        // look in each child
        int i = 0;
        for (i = 0; i < treeHead->numberOfChildren; i++)
        {
            scopeAnalysis(treeHead->children[i]);
        }
    }
}

void printChildren(struct Node *treeHead)
{
    int i = 0;
    for (i = 0; i < treeHead->numberOfChildren; i++)
    {
        printf("%d=", i);
        if (treeHead->children[i]->data != NULL)
        {
            printf("%s, ", treeHead->children[i]->data->text);
        }
        else
        {
            printf("%s, ", treeHead->children[i]->categoryName);
        }
    }
    printf("\n");
}

void handlePackage(struct Node *treeHead)
{
    char *packageName = strdup(treeHead->children[1]->data->text);
    if (strcmp(packageName, "main") != 0)
    {
        printf("Package name must be main in VGo instead found '%s' at %s:%d\n", treeHead->children[1]->data->text, currentfile, treeHead->children[1]->data->linenumber);
        exit(3);
    }
}

void handleImportPackage(struct Node *treeHead)
{
    if (strcmp(treeHead->children[0]->data->sval, "fmt") == 0)
    {
        fmtSymbolTable = createStructTable("fmt", globalSymbolTable);
        int index = calculateHashKey("Println");

        struct Symbol *newData = malloc(sizeof(struct Symbol));
        newData->name = "Println";
        newData->type = function;
        newData->typeName = "function";
        fmtSymbolTable->hash[index] = addToFront(newData, fmtSymbolTable->hash[index]);
    }
    else if (strcmp(treeHead->children[0]->data->sval, "time") == 0)
    {
        timeSymbolTable = createStructTable("time", globalSymbolTable);
        int index = calculateHashKey("Now");

        struct Symbol *newData = malloc(sizeof(struct Symbol));
        newData->name = "Now";
        newData->type = function;
        newData->typeName = "function";
        timeSymbolTable->hash[index] = addToFront(newData, timeSymbolTable->hash[index]);
    }
    else if (strcmp(treeHead->children[0]->data->sval, "math/rand") == 0)
    {
        mathSymbolTable = createStructTable("math/rand", globalSymbolTable);
        int index = calculateHashKey("Intn");

        struct Symbol *newData = malloc(sizeof(struct Symbol));
        newData->name = "Intn";
        newData->type = function;
        newData->typeName = "function";
        mathSymbolTable->hash[index] = addToFront(newData, mathSymbolTable->hash[index]);
    }
    else
    {
        printf("The following package %s is not supported in VGo\n", treeHead->children[0]->data->text);
        exit(3);
    }
}

void handleStruct(struct Node *treeHead)
{
    if (treeHead->numberOfChildren == 2)
    {
        struct symboltable *currentStructTable = createStructTable(treeHead->children[0]->data->text, globalSymbolTable);
        lookForStructVariables(treeHead->children[1], currentStructTable);
    }
}

void handleFunctionDeclaration(struct Node *treeHead)
{
    if (treeHead->children[1]->category == fndcl)
    {
        if (treeHead->children[1]->children[0]->data != NULL)
        {
            currentSymbolTable = createSymbolTable(strdup(treeHead->children[1]->children[0]->data->text), currentSymbolTable);
            addToFunctionList(currentSymbolTable);

            // handle parameters
            if (treeHead->children[1]->children[1] != NULL)
            {

                if (treeHead->children[1]->children[1]->category == oarg_type_list_ocomma)
                {
                    if (treeHead->children[1]->children[1]->children[0] == NULL)
                    {
                    }
                    else
                    {
                        lookForParameterNames(treeHead->children[1]->children[1]->children[0]);
                        handleMissingTypes(currentSymbolTable->declarationPropertyList);
                        insertDeclarationPropertyList(currentSymbolTable);
                    }
                }
            }
            if (treeHead->children[1]->numberOfChildren == 3)
            {
                lookForReturnTypes(treeHead->children[1]->children[2]);
                if (currentSymbolTable->returnType == 0)
                {
                    currentSymbolTable->returnType = VOID;
                    currentSymbolTable->returnTypeName = "void";
                }
            }
            else
            {
                currentSymbolTable->returnType = VOID;
                currentSymbolTable->returnTypeName = "void";
            }
        }
    }

    // continue running on function body if it exists
    if (treeHead->numberOfChildren == 3)
    {
        if (treeHead->children[2]->category == fnbody)
        {
            scopeAnalysis(treeHead->children[2]);
            if (currentSymbolTable->parent != NULL)
            {
                currentSymbolTable = currentSymbolTable->parent;
            }
        }
    }
    else
    {
    }
}

void lookForVariableNames(struct Node *treeHead, int type, char *typeName)
{
    if (treeHead->children[0]->category == dcl_name_list)
    {
        lookForVariableNames(treeHead->children[0], type, typeName);
    }
    if (treeHead->children[0] != NULL && treeHead->children[0]->category == dcl_name)
    {
        if (arraySize != -1)
        {
            treeHead->children[0]->children[0]->data->ival = arraySize;
            arraySize = -1;
        }
        insertVariableIntoHash(treeHead->children[0]->children[0], type, typeName, currentSymbolTable);
    }
    else if (treeHead->children[1] != NULL && treeHead->children[1]->category == dcl_name)
    {
        if (arraySize != -1)
        {
            treeHead->children[1]->children[0]->data->ival = arraySize;

            arraySize = -1;
        }
        insertVariableIntoHash(treeHead->children[1]->children[0], type, typeName, currentSymbolTable);
    }
    else
    {
        printf("Something went wrong here is the tree for debugging\n");
        treeprint(treeHead, 0);
        exit(3);
    }
}

void lookForReturnTypes(struct Node *treeHead)
{
    if (treeHead == NULL)
    {
        // do nothing
    }
    else if (treeHead->numberOfChildren > 0)
    {
        int i = 0;
        for (i = 0; i < treeHead->numberOfChildren; i++)
        {
            lookForReturnTypes(treeHead->children[i]);
        }
    }
    else if (treeHead != NULL)
    {
        currentSymbolTable->returnType = treeHead->data->category;
        currentSymbolTable->returnTypeName = strdup(treeHead->data->text);
    }
    else
    {
        printf("Error found in the following tree\n");
        treeprint(treeHead, 0);
        exit(3);
    }
}

void handleVariableDeclaration(struct Node *treeHead)
{
    int type;
    char *typeName;

    if (treeHead->children[1] != NULL && treeHead->children[1]->numberOfChildren == 0)
    {
        // regular variable declaration
        type = treeHead->children[1]->data->category;
        typeName = strdup(treeHead->children[1]->data->text);
        if (type == LNAME)
        {
        }
        lookForVariableNames(treeHead->children[0], type, typeName);
    }
    else
    {
        // array declaration
        if (treeHead->children[1]->category == othertype)
        {
            if (treeHead->children[1]->children[1] == NULL)
            {
                printf("Array declarations need to have a size on line %d\n", treeHead->children[1]->children[0]->data->linenumber);
                exit(3);
            }
            else
            {
                type = treeHead->children[1]->children[treeHead->children[1]->numberOfChildren - 1]->data->category;
                typeName = strdup(treeHead->children[1]->children[treeHead->children[1]->numberOfChildren - 1]->data->text);
                if (treeHead->children[1]->children[1]->children[0]->data->category == LNAME)
                {
                    printf("Found variable instead of a size in array\n");
                    exit(3);
                }
                else
                {
                    arraySize = treeHead->children[1]->children[1]->children[0]->data->ival;
                    lookForVariableNames(treeHead->children[0], type, typeName);
                }
            }
        }
    }
}

void handlePotentialStructInstance(struct Node *treeHead)
{
    if (treeHead->numberOfChildren >= 3)
    {
        if (treeHead->children[1]->data->category == PERIOD)
        {
            if (strcmp(treeHead->children[0]->children[0]->data->text, "fmt") == 0)
            {
                if (strcmp(treeHead->children[2]->data->text, "Println") != 0)
                {
                    printf("That isn't a Println its a %s\n", treeHead->children[1]->data->text);
                    exit(3);
                }
            }
            else if (strcmp(treeHead->children[0]->children[0]->data->text, "time") == 0)
            {
                if (strcmp(treeHead->children[2]->data->text, "Now") != 0)
                {
                    printf("That isn't a Now its a %s\n", treeHead->children[1]->data->text);
                    exit(3);
                }
            }
            else if (strcmp(treeHead->children[0]->children[0]->data->text, "math/rand") == 0)
            {
                if (strcmp(treeHead->children[2]->data->text, "Intn") != 0)
                {
                    printf("That isn't a Intn its a %s\n", treeHead->children[1]->data->text);
                    exit(3);
                }
            }
            else
            {
                // we found a struct instance
                // int index = calculateHashKey(treeHead->children[2]->data->text);
                int typeName = findTypeInSymbolTable(currentSymbolTable, treeHead->children[0]->children[0]->data->text);
                if (typeName > 0)
                {
                    // struct symboltable *variableSymbolTable = findStructTable(typeName);
                    // int returnIsVariableInTable = isVariableInTable(variableSymbolTable, index, treeHead->children[2]->data->text);
                    // if (returnIsVariableInTable == 1 || returnIsVariableInTable == 2)
                    // {
                    //     // do nothing this is valid
                    // }
                    // else
                    // {
                    //     printf("%s.%s is not in the current scope\n", treeHead->children[0]->children[0]->data->text, treeHead->children[2]->data->text);
                    //     exit(3);
                    // }
                }
                else
                {
                    printf("%s.%s is not in the current scope\n", treeHead->children[0]->children[0]->data->text, treeHead->children[2]->data->text);
                    exit(3);
                }
            }
        }
    }
    else
    {
        checkChildren(treeHead);
    }
}

void lookForParameterNames(struct Node *treeHead)
{
    if (treeHead->category == arg_type)
    {
        if (treeHead->numberOfChildren == 1)
        {
            struct Symbol *newData = malloc(sizeof(struct Symbol));
            newData->name = strdup(treeHead->children[0]->children[0]->data->text);
            newData->type = -1;
            newData->typeName = NULL;
            currentSymbolTable->declarationPropertyList = addToEnd(newData, currentSymbolTable->declarationPropertyList);
        }
        if (treeHead->numberOfChildren == 2)
        {

            int type = treeHead->children[1]->children[0]->data->category;
            char *typeName = treeHead->children[1]->children[0]->data->text;

            struct Symbol *newData = malloc(sizeof(struct Symbol));
            newData->name = strdup(treeHead->children[0]->data->text);
            newData->type = type;
            newData->typeName = strdup(typeName);
            currentSymbolTable->declarationPropertyList = addToEnd(newData, currentSymbolTable->declarationPropertyList);

            insertVariableIntoHash(treeHead->children[0], type, typeName, currentSymbolTable);
        }
    }
    else
    {
        if (treeHead->numberOfChildren > 0)
        {
            // look in each child
            int i = 0;
            for (i = 0; i < treeHead->numberOfChildren; i++)
            {
                lookForParameterNames(treeHead->children[i]);
            }
        }
    }
}

void handleVariableInstance(struct Node *treeHead)
{
    int index = calculateHashKey(treeHead->data->text);
    if (isVariableInTable(currentSymbolTable, index, treeHead->data->text) == 0)
    {
        printf("Undeclared variable '%s' at file %s on line %d encountered\n", treeHead->data->text, treeHead->data->filename, treeHead->data->linenumber);
        exit(3);
    }
}

void lookForStructVariables(struct Node *treeHead, struct symboltable *currentStruct)
{
    if (treeHead == NULL)
    {
        // do nothing
    }
    else if (treeHead->category == structdcl)
    {
        int type = treeHead->children[1]->data->category;
        char *typeName = strdup(treeHead->children[1]->data->text);

        insertVariableIntoHash(treeHead->children[0]->children[0]->children[0], type, typeName, currentStruct);
    }
    else if (treeHead->numberOfChildren > 0)
    {
        int i = 0;
        for (i = 0; i < treeHead->numberOfChildren; i++)
        {
            lookForStructVariables(treeHead->children[i], currentStruct);
        }
    }
}

void handleConst(struct Node *treeHead)
{
    if (treeHead->numberOfChildren == 4)
    {

        if (treeHead->children[1]->numberOfChildren == 0)
        {
            int type;
            char *typeName;
            type = treeHead->children[1]->data->category;
            typeName = strdup(treeHead->children[1]->data->text);

            findConstName(treeHead->children[0], type, typeName);
        }
    }
}

void findConstName(struct Node *treeHead, int type, char *typeName)
{
    if (treeHead == NULL)
    {
        // do nothing
    }
    else if (treeHead->category == dcl_name)
    {
        treeHead->children[0]->category = lconst;
        insertVariableIntoHash(treeHead->children[0], type, typeName, currentSymbolTable);
    }
    else
    {
        int i = 0;
        for (i = 0; i < treeHead->numberOfChildren; i++)
        {
            findConstName(treeHead->children[i], type, typeName);
        }
    }
}

char *getTerminalText(struct Node *treeHead)
{
    if (treeHead != NULL)
    {
        if (treeHead->numberOfChildren == 0)
        {
            return treeHead->data->text;
        }
        else
        {
            return getTerminalText(treeHead->children[0]);
        }
    }
    else
    {
        printf("Empty tree missing variable name\n");
        exit(3);
    }
}

void checkTypeFunctionDeclaration(struct Node *treeHead)
{
    if (treeHead->children[1]->category == fndcl)
    {
        if (treeHead->children[1]->children[0]->data != NULL)
        {
            currentSymbolTable = findSymbolTable(treeHead->children[1]->children[0]->data->text);
        }
    }
    if (treeHead->numberOfChildren == 3)
    {
        if (treeHead->children[2]->category == fnbody)
        {
            checkTypeChildren(treeHead->children[2]);
            if (currentSymbolTable->parent != NULL)
            {
                currentSymbolTable = currentSymbolTable->parent;
            }
        }
    }
}

int checkTypeFunctionCall(struct Node *treeHead)
{
    if (treeHead->numberOfChildren > 0)
    {
        if (treeHead->children[0]->numberOfChildren > 0)
        {
            if (treeHead->children[0]->children[0]->numberOfChildren == 0)
            {
                if (strcmp(treeHead->children[0]->children[0]->data->text, "fmt") == 0)
                {
                    // currentSymbolTable = fmtSymbolTable;
                }
                else if (strcmp(treeHead->children[0]->children[0]->data->text, "time") == 0)
                {
                    // currentSymbolTable = timeSymbolTable;
                }
                else if (strcmp(treeHead->children[0]->children[0]->data->text, "Math/rand") == 0)
                {
                    // currentSymbolTable = mathSymbolTable;
                }
                else
                {
                    currentSymbolTable = findSymbolTable(treeHead->children[0]->children[0]->data->text);
                }
            }
            else if (treeHead->children[0]->children[0]->numberOfChildren == 1)
            {
                char *variableName = getTerminalText(treeHead->children[0]->children[0]);
                if (strcmp(variableName, "fmt") == 0)
                {
                    // currentSymbolTable = fmtSymbolTable;
                    return VOID;
                }
                else if (strcmp(variableName, "time") == 0)
                {
                    // currentSymbolTable = timeSymbolTable;
                    return INT;
                }
                else if (strcmp(variableName, "Math/rand") == 0)
                {
                    // currentSymbolTable = mathSymbolTable;
                    return INT;
                }
                else
                {
                    checkTypeChildren(treeHead->children[0]->children[0]);
                }
            }
            else
            {
                printf("Unable to find function in the following tree\n");
                treeprint(treeHead, 0);
                exit(3);
            }
        }
    }

    // check parameter list
    if (currentSymbolTable->declarationPropertyList != NULL)
    {
        if (treeHead->numberOfChildren == 3)
        {
            printf("There are parameters for function %s but parameters were not provided\n", currentSymbolTable->tablename);
            exit(3);
        }
        else
        {
            struct LinkedListNode *paramTypeHead = malloc(sizeof(struct LinkedListNode));
            paramTypeHead = NULL;
            paramTypeHead = checkParameterTypes(currentSymbolTable, paramTypeHead, treeHead->children[2]);
            if (compareLinkedLists(paramTypeHead, currentSymbolTable->declarationPropertyList) == 0)
            {
                printf("Error called function %s called with a the following types\n", currentSymbolTable->tablename);
                printLinkedList(paramTypeHead);
                exit(3);
            }
        }
    }
    else if (treeHead->numberOfChildren == 5 || treeHead->numberOfChildren == 4)
    {
        printf("There are no parameters for function %s but parameters were provided\n", currentSymbolTable->tablename);
        exit(3);
    }

    // check return type
    return currentSymbolTable->returnType;
}

void checkTypeNonDclStmt(struct Node *treeHead)
{
    int rightType = 0;
    if (treeHead->numberOfChildren == 2)
    {
        if (treeHead->children[0]->numberOfChildren == 0)
        {
            rightType = checkTypeChildren(treeHead->children[1]);
            if (rightType != currentSymbolTable->returnType)
            {
                printf("Return type is not the same as the function return type. Expected %s but got %s\n", findTypeName(currentSymbolTable->returnType), findTypeName(rightType));
                exit(3);
            }
        }
    }
    else
    {
        checkTypeChildren(treeHead);
    }
}

int checkTypeExpression(struct Node *treeHead)
{
    int leftType = 0;
    int rightType = 0;
    leftType = checkTypeChildren(treeHead->children[0]);
    rightType = findTerminal(treeHead->children[2]);
    if (leftType == LNAME || rightType == LNAME)
    {
        printf("Error found type struct on operaion '%s' on line %d\n", treeHead->children[1]->data->text, treeHead->children[1]->data->linenumber);
        exit(3);
    }
    else if (compareLeftAndRightTypes(leftType, rightType))
    {
        switch (treeHead->children[1]->category)
        {
        case LLT:
        case LGT:
        case LLE:
        case LGE:
        case LANDAND:
        case LOROR:
            return BOOL;

        default:
            return leftType;
        }
    }
    else
    {
        printf("Error type '%s' != type '%s' in operation '%s' on line %d\n", findTypeName(leftType), findTypeName(rightType), treeHead->children[1]->data->text, treeHead->children[1]->data->linenumber);
        exit(3);
    }
    exit(3);
    return -1;
}

int checkTypeSimpleStatement(struct Node *treeHead)
{
    int leftType = 0;
    int rightType = 0;
    if (treeHead->numberOfChildren == 1)
    {
        return checkTypeChildren(treeHead);
    }
    else if (treeHead->numberOfChildren == 3)
    {
        leftType = typeAnalysis(treeHead->children[0]);
        rightType = typeAnalysis(treeHead->children[2]);
        if (compareLeftAndRightTypes(leftType, rightType) == 0)
        {
            printf("Error type '%s' != type '%s' in operation '%s' on line %d\n", findTypeName(leftType), findTypeName(rightType), treeHead->children[1]->data->text, treeHead->children[1]->data->linenumber);
            exit(3);
        }
        else
        {
            return leftType;
        }
    }
    return -1;
}

int checkTypepexpr_no_paren(struct Node *treeHead)
{
    if (treeHead->numberOfChildren == 0)
    {
        return typeAnalysis(treeHead);
    }
    if (treeHead->numberOfChildren == 1)
    {
        int type = typeAnalysis(treeHead->children[0]);
        return type;
    }
    else if (treeHead->numberOfChildren == 3)
    {
        if (treeHead->children[1]->numberOfChildren == 0)
        {
            if (treeHead->children[1]->data->category == EQUAL)
            {
                int leftType = typeAnalysis(treeHead->children[0]);
                int rightType = typeAnalysis(treeHead->children[2]);
                if (compareLeftAndRightTypes(leftType, rightType))
                {
                    return leftType;
                }
                else
                {
                    printf("Attempted operation types %s = %s\n", findTypeName(leftType), findTypeName(rightType));
                    exit(3);
                }
            }
            else
            {
                return checkTypeChildren(treeHead);
            }
        }
    }
    else if (treeHead->numberOfChildren == 4)
    {
        if (treeHead->children[1]->numberOfChildren == 0 && treeHead->children[1]->data->category == LSQUAREBRACE)
        {
            if (typeAnalysis(treeHead->children[0]) != typeAnalysis(treeHead->children[2]))
            {
                printf("something bad happened\n");
            }
        }
    }
    else
    {
        checkTypeChildren(treeHead);
    }
    return -1;
}

int checkTypeDefault(struct Node *treeHead)
{
    // either return a type of a variable or return a type from the children
    if (treeHead->numberOfChildren == 0)
    {
        if (treeHead->data->category == LNAME)
        {
            int variableType = findTypeInSymbolTable(currentSymbolTable, treeHead->data->text);
            return variableType;
        }
        else
        {
            return treeHead->data->category;
        }
    }
    else if (treeHead->numberOfChildren == 1)
    {
        return checkTypeChildren(treeHead);
    }
    else
    {
        checkTypeChildren(treeHead);
    }
    return -1;
}

void checkForHeader(struct Node *treeHead)
{
    if (treeHead->numberOfChildren == 1)
    {
        if (treeHead->children[0] == NULL)
        {
            // do nothing
        }
        else
        {
            if (typeAnalysis(treeHead->children[0]) != BOOL)
            {
                printf("Error conditional does not have type BOOL instead found the following tree\n");
                treeprint(treeHead->children[0], 0);
                exit(3);
            }
            else
            {
                // do nothing
            }
        }
    }
    else if (treeHead->children[1]->numberOfChildren == 0 && treeHead->children[1]->data->category == SEMICOLON)
    {
        if (typeAnalysis(treeHead->children[2]) != BOOL)
        {
            printf("Error conditional does not have type BOOL instead found the following tree\n");
            treeprint(treeHead->children[2], 0);
            exit(3);
        }
        else
        {
            // do nothing
        }
    }
    else
    {
        if (typeAnalysis(treeHead->children[1]) != BOOL)
        {
            printf("Error conditional does not have type BOOL instead found the following tree\n");
            treeprint(treeHead->children[1], 0);
            exit(3);
        }
        else
        {
            // do nothing
        }
    }
}

void generateLabels(struct Node *treeHead, int region, int *regionOffset)
{

    if (treeHead != NULL)
    {
        switch (treeHead->category)
        {
        case xfndcl:
            addLabelsToFunction(treeHead);
            break;
        case expr:
            treeHead->address = createLocation(region, regionOffset);
            checkLabelChildren(treeHead, region, regionOffset);
            break;

        case NUMERICLITERAL:
        case OCTAL:
        case HEXADECIMAL:
            treeHead->address = createLocation(REGIONCONST, &offsetCONST);
            break;

        case DECIMAL:
        case SCIENTIFICNUM:
            treeHead->address = createLocation(REGIONCONST, &offsetCONST);
            break;

        case STRINGLIT:
        case CHAR:
            treeHead->address = createLocation(REGIONSTRING, &offsetSTRING);
            break;

        case LNAME:
            treeHead->address = findLocationFromSymbolTable(currentSymbolTable, treeHead->data->text);
            break;

        case for_body:

            handleForLocation(treeHead, region, regionOffset);
            break;

        case if_stmt:
            handleIfLocation(treeHead, region, regionOffset);
            break;

        default:
            checkLabelChildren(treeHead, region, regionOffset);
            break;
        }
    }
}

void checkLabelChildren(struct Node *treeHead, int region, int *regionOffset)
{
    if (treeHead != NULL)
    {
        int i = 0;
        for (i = 0; i < treeHead->numberOfChildren; i++)
        {
            generateLabels(treeHead->children[i], region, regionOffset);
        }
    }
}

void addLabelsToFunctionSymbolTable(struct symboltable *currentFunctionSymbolTable)
{
    int i = 0;
    for (i = 0; i < 701; i++)
    {
        addLabelsToLinkedList(currentFunctionSymbolTable->hash[i]);
    }
}

void addLabelsToLinkedList(struct LinkedListNode *head)
{
    struct LinkedListNode *current = head;
    while (current != NULL)
    {
        current->data->address = createLocation(REGIONFUNCTION, &offsetFUNCTION);
        current = current->next;
    }
}

void addLabelsToFunction(struct Node *treeHead)
{
    treeHead->first = createLocation(REGIONFUNCTION, &offsetFUNCTION);
    if (treeHead->numberOfChildren > 0)
    {
        if (treeHead->children[1]->category == fndcl && treeHead->children[1]->children[0]->numberOfChildren == 0)
        {
            struct symboltable *currentFunctionSymbolTable = findSymbolTable(treeHead->children[1]->children[0]->data->text);
            currentFunctionSymbolTable->address = treeHead->first;
            addLabelsToFunctionSymbolTable(currentFunctionSymbolTable);
            if (currentFunctionSymbolTable->returnType != VOID)
            {
                currentFunctionSymbolTable->returnValue = createLocation(REGIONFUNCTION, &offsetFUNCTION);
            }
        }
        if (treeHead->children[2]->category == fnbody)
        {
            checkLabelChildren(treeHead->children[2], REGIONFUNCTION, &offsetFUNCTION);

            // after we get the end of the function
            treeHead->follow = createLocation(REGIONFUNCTION, &offsetFUNCTION);
        }
    }
}

void handleIfLocation(struct Node *treeHead, int region, int *regionOffset)
{
    // setup each address location and then connect the nodes to the parent
    if (treeHead->numberOfChildren >= 3 && treeHead->children[2]->category == loop_body)
    {
        treeHead->children[2]->address = createLocation(region, regionOffset);
        treeHead->children[1]->ifTrue = treeHead->children[2]->address;
        checkLabelChildren(treeHead->children[2], region, regionOffset);
        treeHead->children[2]->follow = createLocation(region, regionOffset);
    }
    if (treeHead->numberOfChildren == 4 && treeHead->children[3]->category == nonterminal_else)
    {
        treeHead->children[3]->address = createLocation(region, regionOffset);
        treeHead->children[1]->ifFalse = treeHead->children[3]->address;
        checkLabelChildren(treeHead->children[3], region, regionOffset);
    }
    else if (treeHead->numberOfChildren >= 5 && treeHead->children[4]->category == nonterminal_else)
    {
        treeHead->children[4]->address = createLocation(region, regionOffset);
        treeHead->children[1]->ifFalse = treeHead->children[4]->address;
        checkLabelChildren(treeHead->children[4], region, regionOffset);
    }
}

void handleForLocation(struct Node *treeHead, int region, int *regionOffset)
{
    treeprint(treeHead, 0);
    if (treeHead->numberOfChildren == 2)
    {
        // handle header
        // treeHead->children[0]
        if (treeHead->children[0] != NULL)
        {
            // handle before loop
            if (treeHead->children[0]->children[0] != NULL && treeHead->children[0]->children[0]->category == osimple_stmt)
            {
                treeHead->children[0]->children[0]->address = createLocation(region, regionOffset);
                checkLabelChildren(treeHead->children[0]->children[0], region, regionOffset);
            }
            else if (treeHead->children[0]->children[1] != NULL && treeHead->children[0]->children[1]->category == osimple_stmt)
            {
                treeHead->children[0]->children[1]->address = createLocation(region, regionOffset);
                checkLabelChildren(treeHead->children[0]->children[1], region, regionOffset);
            }

            // handle loop conditional
            if (treeHead->children[0]->children[1] != NULL && treeHead->children[0]->children[1]->category != SEMICOLON)
            {
                treeHead->children[0]->children[1]->address = createLocation(region, regionOffset);
                checkLabelChildren(treeHead->children[0]->children[1], region, regionOffset);
            }
            else if (treeHead->children[0]->children[2] != NULL && treeHead->children[0]->children[2]->category == osimple_stmt)
            {
                treeHead->children[0]->children[2]->address = createLocation(region, regionOffset);
                checkLabelChildren(treeHead->children[0]->children[2], region, regionOffset);
            }

            // handle iteration
            if (treeHead->children[0]->numberOfChildren == 5)
            {
                treeHead->children[0]->children[4]->address = createLocation(region, regionOffset);
                checkLabelChildren(treeHead->children[0]->children[4], region, regionOffset);
            }
        }

        // loop body
        if (treeHead->children[1] != NULL)
        {
            // start of loop body
            treeHead->children[1]->address = createLocation(region, regionOffset);
            checkLabelChildren(treeHead->children[1], region, regionOffset);
        }
    }
}

struct operation *beginIntermediateCodeGeneration(struct Node *treeHead)
{
    struct operation *head = NULL;
    if (treeHead != NULL)
    {
        switch (treeHead->category)
        {

        case xfndcl:
            return generateFunctionOperation(treeHead);
            break;

        case pseudocall:
            return callFunction(treeHead);

        case expr:
            return generateExpressionOperation(treeHead);
            break;

        case simple_stmt:
            return generateSimpleStmtOperation(treeHead);
            break;

        default:
            return checkCodeGenerationChildren(treeHead);
            break;
        }
    }
    return head;
}

struct operation *checkCodeGenerationChildren(struct Node *treeHead)
{
    struct operation *head = NULL;
    int i = 0;
    for (i = 0; i < treeHead->numberOfChildren; i++)
    {
        struct operation *right = beginIntermediateCodeGeneration(treeHead->children[i]);
        head = combineLinkedList(head, right);
    }
    return head;
}

struct operation *generateFunctionOperation(struct Node *treeHead)
{
    struct operation *head = createOperation(D_LABEL, NULL, NULL, treeHead->first);
    struct operation *right = checkCodeGenerationChildren(treeHead);
    head = combineLinkedList(head, right);
    head = combineLinkedList(head, createOperation(O_RET, NULL, NULL, treeHead->follow));
    return head;
}

struct operation *generateExpressionOperation(struct Node *treeHead)
{
    struct operation *head = NULL;
    if (treeHead != NULL)
    {
        int opcode = 0;

        switch (treeHead->children[1]->category)
        {
        case PLUS:
            opcode = O_ADD;
            break;
        case MINUS:
            opcode = O_SUB;
            break;
        case STAR:
            opcode = O_MUL;
            break;
        case DIVIDE:
            opcode = O_DIV;
            break;
        case MOD:
            // ~~~ generate mod
            break;

        case LEQ:
            opcode = O_BEQ;
            break;
        case LNE:
            opcode = O_NEG;
            break;
        case LLT:
            opcode = O_BLT;
            break;
        case LGT:
            opcode = O_BGT;
            break;
        case LLE:
            opcode = O_BLE;
            break;
        case LGE:
            opcode = O_BGE;
            break;

        default:
            printf("error is not defined %d\n", treeHead->children[1]->category);
            exit(4);
            break;
        }
        struct location *leftAddress = NULL, *rightAddress = NULL;
        if (treeHead->children[0]->address != NULL)
        {
            leftAddress = treeHead->children[0]->address;
        }
        else if (treeHead->children[0]->children[0]->address != NULL)
        {
            leftAddress = treeHead->children[0]->children[0]->address;
        }
        else
        {
            // ~~~ potential bug
        }

        if (treeHead->children[2]->address != NULL)
        {
            rightAddress = treeHead->children[2]->address;
            // right = checkCodeGenerationChildren(treeHead->children[2]);
        }
        else if (treeHead->children[2]->children[0]->address != NULL)
        {
            rightAddress = treeHead->children[2]->children[0]->address;
        }

        head = createOperation(opcode, leftAddress, rightAddress, treeHead->address);
        head = combineLinkedList(head, checkCodeGenerationChildren(treeHead));
    }
    return head;
}

struct operation *generateSimpleStmtOperation(struct Node *treeHead)
{
    struct operation *head = NULL;
    int opcode = 0;
    struct location *leftAddress = NULL, *rightAddress = NULL, *destination = treeHead->address;
    if (treeHead != NULL)
    {
        if (treeHead->numberOfChildren == 3)
        {
            // general case
            switch (treeHead->children[1]->category)
            {
            case EQUAL:
                opcode = O_ASN;
                if (treeHead->children[0]->address != NULL)
                {
                    destination = treeHead->children[0]->address;
                }
                else if (treeHead->children[0]->children[0]->address != NULL)
                {
                    destination = treeHead->children[0]->children[0]->address;
                }
                if (treeHead->children[2]->address != NULL)
                {
                    leftAddress = treeHead->children[2]->address;
                }
                else if (treeHead->children[2]->children[0]->address != NULL)
                {
                    leftAddress = treeHead->children[2]->children[0]->address;
                }
                break;
            case LASOP:
                opcode = O_ADD1;
                if (treeHead->children[0]->address != NULL)
                {
                    destination = treeHead->children[0]->address;
                }
                else if (treeHead->children[0]->children[0]->address != NULL)
                {
                    destination = treeHead->children[0]->children[0]->address;
                }
                else
                {
                    // ~~~ bug
                }
                if (treeHead->children[0]->address != NULL)
                {
                    leftAddress = treeHead->children[0]->address;
                }
                else if (treeHead->children[0]->children[0]->address != NULL)
                {
                    leftAddress = treeHead->children[0]->children[0]->address;
                }

                if (treeHead->children[2] == NULL)
                {
                    // do nothing
                }
                else if (treeHead->children[2]->address != NULL)
                {
                    rightAddress = treeHead->children[2]->address;
                }
                else if (treeHead->children[2]->children[0]->address != NULL)
                {
                    rightAddress = treeHead->children[2]->children[0]->address;
                }

                break;

            default:
                printf("error is not defined %d\n", treeHead->children[1]->category);
                exit(4);
                break;
            }
        }
        else if (treeHead->numberOfChildren == 2)
        {
            // we have a uniary operation like ++
            switch (treeHead->children[1]->category)
            {
            case LINC:
                opcode = O_ADD1;
                break;
            case LDEC:
                opcode = O_SUB1;
                break;

            default:
                printf("error is not defined %d\n", treeHead->children[1]->category);
                exit(4);
                break;
            }
            if (treeHead->children[0]->address != NULL)
            {
                leftAddress = treeHead->children[0]->address;
            }
            else if (treeHead->children[0]->children[0]->address != NULL)
            {
                leftAddress = treeHead->children[0]->children[0]->address;
            }
        }
        head = createOperation(opcode, leftAddress, rightAddress, destination);
        head = combineLinkedList(head, checkCodeGenerationChildren(treeHead));
    }
    return head;
}

struct operation *callFunction(struct Node *treeHead)
{
    struct operation *head = NULL;
    char *functionName = "";
    struct location *address = NULL;
    struct symboltable *functionTable = NULL;
    if (treeHead->children[0]->children[0]->category == LNAME)
    {
        functionName = treeHead->children[0]->children[0]->data->text;
        functionTable = findFunctionLocation(functionName);
        address = functionTable->address;
    }
    else
    {
        printf("we haven't figured out built in functions yet\n");
        // ~~~ handle packages
    }
    head = createOperation(O_GOTO, NULL, NULL, address);

    if (functionTable->declarationPropertyList != NULL)
    {
        // ~~~ finish param list
        // struct operation *paramAssignment = NULL;
        // struct LinkedListNode *current = functionTable->declarationPropertyList;
        // struct location *paramValue = NULL;
        // int paramNumber = 0;
        // while (current != NULL)
        // {
        //     // paramValue = getNextParamValue(treeHead->children[4], paramNumber);
        //     // paramAssignment(O_ASN, )
        //     current = current->next;
        // }
    }

    head = combineLinkedList(head, checkCodeGenerationChildren(treeHead));

    return head;
}

// struct location *getNextParamValue(struct Node *treeHead, int paramNumber){
//     if(treeHead != NULL){
//         if(treeHead->address != NULL ){
//             return treeHead->address;
//         }else{
//             return getNextParamValue(treeHead->)
//         }
//     }else{
//         return NULL;
//     }
// }
