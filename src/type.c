#include <stdlib.h>
#include <stdbool.h>

#include "5cc.h"


Type *ty_int = &(Type){.kind = TY_INT};

Type *NewTypePTR2(Type *base) {
    Type *new = calloc(1, sizeof(Type));
    new->kind = TY_PTR;
    new->base = base;
    return new;
}

bool IsTypeInteger(Type *ty) {
    return ty->kind == TY_INT;
}

void AddType(Node *node) {
    if (!node || node->type)
        return;
    AddType(node->lhs);
    AddType(node->rhs);
    AddType(node->_else);
    AddType(node->cond);
    AddType(node->inc);
    AddType(node->init);
    AddType(node->then);

    for (Node *n = node->body; n; n = n->next)
        AddType(n);

    switch (node->kind) {
    case ND_ADD:
    case ND_SUB:
    case ND_MUL:
    case ND_DIV:
    case ND_NEG:
    case ND_ASSIGN:
        node->type = node->lhs->type;
        return;
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
    case ND_NUM:
        node->type = ty_int;
        return;
    case ND_ADDR:
        node->type = NewTypePTR2(node->lhs->type);
        return;
    case ND_DEREF:
        if (node->lhs->type->kind != TY_PTR)
            Error("invalid pointer dereference");
        node->type = node->lhs->type->base;
        return;
    }
}