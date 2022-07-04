#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "5cc.h"

bool IsTokenEqual(Token *tok, char *op) {
  return tok->kind == TK_RESERVED && memcmp(tok->str, op, tok->len) == 0 && op[tok->len] == '\0';
}

Token *SkipToken(Token *tok, char *s) {
  if (!IsTokenEqual(tok, s))
    Error("expected '%s'", s);
  return tok->next;
}

int ExpectTokenNum(Token **tok) {
    if ((*tok)->kind != TK_NUM)
        Error("数ではありません");
    int val = (*tok)->val;
    *tok = (*tok)->next;
    return val;
}

bool IsTokenAtEof(Token *tok) {
  return tok->kind == TK_EOF;
}

Node *NewNodeKind(NodeKind kind) {
    Node *new = calloc(sizeof(Node), 1);
    new->kind = kind;
    return new;
}

Node *NewNodeBinary(NodeKind kind, Node *lhs, Node *rhs) {
    Node *new = NewNodeKind(kind);
    new->lhs = lhs;
    new->rhs = rhs;
    return new;
}

Node *NewNodeNum(int val) {
    Node *new = NewNodeKind(ND_NUM);
    new->val = val;
    return new;
}
Node *NewNodeUnary(NodeKind kind, Node *lhs) {
    Node *new = NewNodeKind(kind);
    new->lhs = lhs;
    return new;
}

//===================================================================
// expr       = equality
// equality   = relational ("==" relational | "!=" relational)*
// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
// mul = unary ("*" unary | "/" unary)*
// unary   = ("+" | "-")? primary
// primary = "(" expr ")" | num
//===================================================================
Node *primary(Token **rest, Token *tok);
Node *unary(Token **rest, Token *tok);
Node *mul(Token **rest, Token *tok);
Node *add(Token **rest, Token *tok);
Node *relational(Token **rest, Token *tok);
Node *equality(Token **rest, Token *tok);
extern Node *expr(Token **rest, Token *tok);
//===================================================================

Node *expr(Token **rest, Token *tok) {
    return equality(rest, tok);
}

Node *equality(Token **rest, Token *tok) {
    Node *node = relational(&tok, tok);

    for (;;) {
        if (IsTokenEqual(tok, "==")) {
            node = NewNodeBinary(ND_EQ, node, add(&tok, tok->next));
            continue;
        }
        if (IsTokenEqual(tok, "!=")) {
            node = NewNodeBinary(ND_NE, node, add(&tok, tok->next));
            continue;
        }
        *rest = tok;
        return node;
    }
}

Node *relational(Token **rest, Token *tok) {
    Node *node  = add(&tok, tok);

    for (;;) {
        if (IsTokenEqual(tok, "<=")) {
            node = NewNodeBinary(ND_LE, node, add(&tok, tok->next));
            continue;
        }
        if (IsTokenEqual(tok, "<")) {
            node = NewNodeBinary(ND_LT, node, add(&tok, tok->next));
            continue;
        }
        if (IsTokenEqual(tok, ">=")) {
            node = NewNodeBinary(ND_LE,  add(&tok, tok->next), node);
            continue;
        }
        if (IsTokenEqual(tok, ">")) {
            node = NewNodeBinary(ND_LT, add(&tok, tok->next), node);
            continue;
        }
        *rest = tok;
        return node;
    }
}

Node *add(Token **rest, Token *tok) {
    Node *node = mul(&tok, tok);

    for (;;) {
        if (IsTokenEqual(tok, "+")) {
            node = NewNodeBinary(ND_ADD, node, mul(&tok, tok->next));
            continue;
        }
        if (IsTokenEqual(tok, "-")) {
            node = NewNodeBinary(ND_SUB, node, mul(&tok, tok->next));
            continue;
        }
        *rest = tok;
        return node;
    }
}

Node *mul(Token **rest, Token *tok) {
    Node *node = unary(&tok, tok);

    for (;;) {
        if (IsTokenEqual(tok, "*")) {
            node = NewNodeBinary(ND_MUL, node, unary(&tok, tok->next));
            continue;
        }
        if (IsTokenEqual(tok, "/")) {
            node = NewNodeBinary(ND_DIV, node, unary(&tok, tok->next));
            continue;
        }
        *rest = tok;
        return node;
    }
}

Node *unary(Token **rest, Token *tok) {
    if (IsTokenEqual(tok, "+")) {
        return unary(rest, tok->next);
    }
    if (IsTokenEqual(tok, "-")) {
        return NewNodeUnary(ND_NEG, unary(rest, tok->next));
    }
    return primary(rest, tok);
}

Node *primary(Token **rest, Token *tok) {
    if (IsTokenEqual(tok, "(")) {
        Node *node = expr(&tok, tok->next);
        *rest = SkipToken(tok, ")");
        return node;
    }
    if (tok->kind == TK_NUM) {
        Node *node = NewNodeNum(tok->val);
        *rest = tok->next;
        return node;
    }
    Error("Something is wrong");
}
