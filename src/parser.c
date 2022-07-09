#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "5cc.h"

static bool IsTokenEqual(Token *tok, char *op) {
  return tok->kind == TK_RESERVED && memcmp(tok->str, op, tok->len) == 0 && op[tok->len] == '\0';
}

static Token *SkipToken(Token *tok, char *s) {
  if (!IsTokenEqual(tok, s))
    Error("expected '%s'", s);
  return tok->next;
}

static bool IsTokenAtEof(Token *tok) {
  return tok->kind == TK_EOF;
}

static Node *NewNodeKind(NodeKind kind) {
    Node *new = calloc(sizeof(Node), 1);
    new->kind = kind;
    return new;
}

static Node *NewNodeBinary(NodeKind kind, Node *lhs, Node *rhs) {
    Node *new = NewNodeKind(kind);
    new->lhs = lhs;
    new->rhs = rhs;
    return new;
}

static Node *NewNodeNum(int val) {
    Node *new = NewNodeKind(ND_NUM);
    new->val = val;
    return new;
}
static Node *NewNodeUnary(NodeKind kind, Node *lhs) {
    Node *new = NewNodeKind(kind);
    new->lhs = lhs;
    return new;
}
static Node *NewNodeVar(char name) {
    Node *new = NewNodeKind(ND_VAR);
    new->name = name;
    return new;
}
//===================================================================
// stmt       = expr_stmt
// expr_stmt  = expr ";"
// expr       = assign 
// assign     = equality ("=" assign)?
// equality   = relational ("==" relational | "!=" relational)*
// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
// mul = unary ("*" unary | "/" unary)*
// unary   = ("+" | "-")? primary
// primary = "(" expr ")" | num
//===================================================================
static Node *primary(Token **rest, Token *tok);
static Node *unary(Token **rest, Token *tok);
static Node *mul(Token **rest, Token *tok);
static Node *add(Token **rest, Token *tok);
static Node *relational(Token **rest, Token *tok);
static Node *equality(Token **rest, Token *tok);
static Node *assign(Token **rest, Token *tok);
static Node *expr(Token **rest, Token *tok);
static Node *expr_stmt(Token **rest, Token *tok);
static Node *stmt(Token **rest, Token *tok);
//===================================================================
static Node *stmt(Token **rest, Token *tok) {
    return expr_stmt(rest, tok);
}


static Node *expr_stmt(Token **rest, Token *tok) {
    Node *node = NewNodeUnary(ND_EXPR_STMT, expr(&tok, tok));
   *rest = SkipToken(tok, ";");
    return node;
}

static Node *expr(Token **rest, Token *tok) {
    return assign(rest, tok);
}

static Node *assign(Token **rest, Token *tok) {
    Node *node = equality(&tok, tok);

    
    if (IsTokenEqual(tok, "=")) {
        node = NewNodeBinary(ND_ASSIGN, node, assign(&tok, tok->next));
    }
    *rest = tok;
    return node;
}

static Node *equality(Token **rest, Token *tok) {
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

static Node *relational(Token **rest, Token *tok) {
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

static Node *add(Token **rest, Token *tok) {
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

static Node *mul(Token **rest, Token *tok) {
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

static Node *unary(Token **rest, Token *tok) {
    if (IsTokenEqual(tok, "+")) {
        return unary(rest, tok->next);
    }
    if (IsTokenEqual(tok, "-")) {
        return NewNodeUnary(ND_NEG, unary(rest, tok->next));
    }
    return primary(rest, tok);
}

static Node *primary(Token **rest, Token *tok) {
    if (IsTokenEqual(tok, "(")) {
        Node *node = expr(&tok, tok->next);
        *rest = SkipToken(tok, ")");
        return node;
    }

    if (tok->kind == TK_INDENT) {
        Node *node = NewNodeVar(*(tok->str));
        *rest = tok->next;
        return node;
    }

    if (tok->kind == TK_NUM) {
        Node *node = NewNodeNum(tok->val);
        *rest = tok->next;
        return node;
    }
    Error("Something is wrong");
}

Node *ParseToken(Token *tok) {
    Node head  = {};
    Node *cur = &head;

    while (!IsTokenAtEof(tok)) {
        cur = cur->next = stmt(&tok, tok); 
    }
    return head.next;
}