#include <stdlib.h>
#include <stdbool.h>

#include "5cc.h"


Type *ty_int = &(Type){.kind = TY_INT, .size = 4, .align = 4};
Type *ty_char = &(Type){.kind = TY_CHAR, .size = 1, .align =1};

Type *NewType(TypeKind kind, int size, int align) {
    Type *new = calloc(1, sizeof(Type));
    new->kind = kind;
    new->size = size;
    new->align = align;
    return new;
}

Type *NewTypePTR2(Type *base) {
    Type *new = NewType(TY_PTR, 8, 8);
    new->base = base;
    return new;
}

Type *NewTypeFn(Type *return_type) {
    Type *new = calloc(1, sizeof(Type));
    new->kind = TY_FN;
    new->return_type = return_type;
    return new;
}

Type *NewTypeArrayOf(Type *base, int len) {
    Type *new = NewType(TY_ARRAY, base->size * len, base->align);
    new->base = base;
    new->array_len = len;
    return new;
}

bool IsTypeInteger(Type *ty) {
    return ty->kind == TY_INT || ty->kind == TY_CHAR;
}

Type *CopyType(Type *ty) {
    Type *ret = calloc(1, sizeof(Type));
    *ret = *ty;
    return ret;
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

    for (Node *n = node->args; n; n = n->next)
        AddType(n);

    switch (node->kind) {
    case ND_ADD:
    case ND_SUB:
    case ND_MUL:
    case ND_DIV:
    case ND_NEG:
        node->type = node->lhs->type;
        return;

    case ND_ASSIGN:
        if (node->lhs->type->kind == TY_ARRAY)
            ErrorToken(node->tok, "not an lvalue");
        node->type = node->lhs->type;
        return;
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
    case ND_NUM:
    case ND_FNCALL:
        node->type = ty_int;
        return;
    case ND_DOTS:
        node->type = node->member->type;
        return;
    case ND_VAR:
        node->type = node->var->type;
        return;
    case ND_COMMA:
        node->type = node->rhs->type;
        return;
    case ND_ADDR:
        if (node->lhs->type->kind == TY_ARRAY)
            node->type = NewTypePTR2(node->lhs->type->base);
        else
            node->type = NewTypePTR2(node->lhs->type);
        return;
    case ND_DEREF:
        if (!node->lhs->type->base)
            ErrorToken(node->tok, "invalid pointer dereference");
        node->type = node->lhs->type->base;
        return;

    case ND_STMT_EXPR:
        if (node->body) {
            Node *stmt = node->body;
            while (stmt->next)
                stmt = stmt->next;
            if (stmt->kind == ND_EXPR_STMT) {
                node->type = stmt->lhs->type;
            return;
            }
        }
        ErrorToken(node->tok, "statement expression returning void is not supported");
        return;
    }
}