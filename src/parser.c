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

static bool ConsumeToken(Token **tok, char *op) {
    if (IsTokenEqual(*tok, op)) {
        *tok = (*tok)->next;
        return true;
    }
    return false;
}


static bool IsTokenAtEof(Token *tok) {
  return tok->kind == TK_EOF;
}

Obj *locals;

static Obj *NewObj() {
    Obj *new = calloc(1, sizeof(Obj));
    return new;
}

static Obj *NewObjLVar(char *name) {
    Obj *new = NewObj();
    new->name = name;
    new->next = locals;
    locals = new;
    return new;
}

static Obj *FindObjLVar(Token *tok) {
    for (Obj *var = locals; var; var = var->next)
        if (strlen(var->name) == tok->len && !strncmp(tok->str, var->name, tok->len))
            return var;
    return NULL;
}


static Node *NewNodeKind(NodeKind kind) {
    Node *new = calloc(1, sizeof(Node));
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
static Node *NewNodeVar(Obj *var) {
    Node *new = NewNodeKind(ND_VAR);
    new->var = var;
    return new;
}

//===================================================================
// compound_stmt = "{" stmt* "}"
// stmt       = expr_stmt || "return" expr ";" || "if" "(" expr ")" stmt ("else" stmt)? || 
//              "for" "(" expr? ";" expr? ";" expr ")" stmt || "while" "(" expr ")" stmt ||
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
static Node *compound_stmt(Token **rest, Token *tok);
static Node *stmt(Token **rest, Token *tok);
//===================================================================


static Node *stmt(Token **rest, Token *tok) {
    if (IsTokenEqual(tok, "return")) {
        Node *node = NewNodeUnary(ND_RETURN, expr(&tok, tok->next));
        *rest = SkipToken(tok, ";");
        return node;
    }
    if (IsTokenEqual(tok, "{")) {
        return compound_stmt(rest, tok->next);
    }
    if (IsTokenEqual(tok, "if")) {
        Node *node = NewNodeKind(ND_IF);
        tok = SkipToken(tok->next, "(");
        node->cond = expr(&tok, tok);
        tok = SkipToken(tok, ")");
        node->then = stmt(&tok, tok);
        if (IsTokenEqual(tok, "else"))
            node->_else = stmt(&tok, tok->next);
        *rest = tok;
        return node;
    }
    if (IsTokenEqual(tok, "for")) {
        Node *node = NewNodeKind(ND_FOR);
        tok = SkipToken(tok->next, "(");

        if (!ConsumeToken(&tok, ";")) {
            node->init = expr(&tok, tok);
            tok = SkipToken(tok, ";");
        }
        if (!ConsumeToken(&tok, ";")) {
            node->cond = expr(&tok, tok);
            tok = SkipToken(tok, ";");
        }
        if (!ConsumeToken(&tok, ")")) {
            node->inc = expr(&tok, tok);
            tok = SkipToken(tok, ")");
        }
        node->then = stmt(&tok, tok);
        *rest = tok;
        return node;
    }
    if (IsTokenEqual(tok, "while")) {
        Node *node = NewNodeKind(ND_FOR);
        tok = SkipToken(tok->next, "(");
        node->cond = expr(&tok, tok);
        tok = SkipToken(tok, ")");
        node->then = stmt(&tok, tok);
        *rest = tok;
        return node;
    }
    return expr_stmt(rest, tok);
}

static Node *compound_stmt(Token **rest, Token *tok) {
    Node head  = {};
    Node *cur = &head;

    while (!IsTokenEqual(tok, "}")) {
        cur = cur->next = stmt(&tok, tok);
        AddType(cur);
    }
    Node *node = NewNodeKind(ND_BLOCK);
    node->body = head.next;
    *rest = tok->next;
    return node;
}

static Node *expr_stmt(Token **rest, Token *tok) {
    if (IsTokenEqual(tok, ";")) {
        *rest = SkipToken(tok, ";");
        return NewNodeKind(ND_BLOCK);
    }
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
static Node *NewNodeAdd(Node *lhs, Node *rhs) {
    AddType(lhs);
    AddType(rhs);

    if (IsTypeInteger(lhs->type) && IsTypeInteger(rhs->type))
        return NewNodeBinary(ND_ADD, lhs, rhs);

    if (lhs->type->base && rhs->type->base)
        Error("can't add ptr to ptr.");
    
    if (!lhs->type->base && rhs->type->base)
        return NewNodeAdd(rhs, lhs);

    rhs = NewNodeBinary(ND_MUL, rhs, NewNodeNum(8));
    return NewNodeBinary(ND_ADD, lhs, rhs);
}

static Node *NewNodeSub(Node *lhs, Node *rhs) {
    AddType(lhs);
    AddType(rhs);

    if (IsTypeInteger(lhs->type) && IsTypeInteger(rhs->type))
        return NewNodeBinary(ND_SUB, lhs, rhs);

    if (lhs->type->base && rhs->type->base) {
        lhs = NewNodeBinary(ND_SUB, lhs, rhs);
        lhs->type = ty_int;
        return NewNodeBinary(ND_DIV, lhs, NewNodeNum(8)); 
    }
    
    if (lhs->type->base && IsTypeInteger(rhs->type)) {
        rhs = NewNodeBinary(ND_MUL, rhs, NewNodeNum(8));
        return NewNodeBinary(ND_SUB, lhs, rhs);
    }
    Error("invalid operands");
}

static Node *add(Token **rest, Token *tok) {
    Node *node = mul(&tok, tok);

    for (;;) {
        if (IsTokenEqual(tok, "+")) {
            //node = NewNodeBinary(ND_ADD, node, mul(&tok, tok->next));
            node = NewNodeAdd(node, mul(&tok, tok->next));
            continue;
        }
        if (IsTokenEqual(tok, "-")) {
            //node = NewNodeBinary(ND_SUB, node, mul(&tok, tok->next));
            node =NewNodeSub(node, mul(&tok, tok->next));
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
    if (IsTokenEqual(tok, "*")) {
        return NewNodeUnary(ND_DEREF, unary(rest, tok->next));
    }
    if (IsTokenEqual(tok, "&")) {
        return NewNodeUnary(ND_ADDR, unary(rest, tok->next));
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
        Obj *var = FindObjLVar(tok);
        if (!var)
            var = NewObjLVar(strndup(tok->str, tok->len));
        *rest = tok->next;
        return NewNodeVar(var);
        
    }

    if (tok->kind == TK_NUM) {
        Node *node = NewNodeNum(tok->val);
        *rest = tok->next;
        return node;
    }
    Error("Something is wrong");
}

Obj *ParseToken(Token *tok) {
    tok = SkipToken(tok, "{");
    Node *body = compound_stmt(&tok, tok);
    Obj *func = NewObj();
    func->locals = locals;
    func->body = body;
    func->is_func = true;
    return func;
}