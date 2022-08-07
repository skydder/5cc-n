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

static bool ConsumeToken(Token **rest, Token *tok, char *op) {
    if (IsTokenEqual(tok, op)) {
        *rest = tok->next;
        return true;
    }
    *rest = tok;
    return false;
}

static bool IsTokenAtEof(Token *tok) {
  return tok->kind == TK_EOF;
}

static Obj *locals;
static Obj *globals;

static Obj *NewObj() {
    Obj *new = calloc(1, sizeof(Obj));
    return new;
}

static Obj *NewObjVar(char *name, Type *type) {
    Obj *new = NewObj();
    new->name = name;
    new->type = type;
    return new;
}

static Obj *NewObjLVar(char *name, Type *type) {
    Obj *new = NewObjVar(name, type);
    new->is_lvar = true;
    new->next = locals;
    locals = new;
    return new;
}

static Obj *NewObjGVar(char *name, Type *type) {
    Obj *new = NewObjVar(name, type);
    new->is_lvar = false;
    new->next = globals;
    globals = new;
    return new;
}

static Obj *FindObjVar(Token *tok) {
    for (Obj *var = locals; var; var = var->next)
        if (strlen(var->name) == tok->len && !strncmp(tok->str, var->name, tok->len))
            return var;
    for (Obj *var = globals; var; var = var->next)
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
    new->type = var->type;
    return new;
}

static char *GetTokenIdent(Token *tok) {
    if (tok->kind != TK_IDENT)
        Error("This is not ident");
    return strndup(tok->str, tok->len);
}

static int GetTokenNum(Token *tok) {
    if (tok->kind != TK_NUM)
        Error("This is not number");
    return tok->val;
}

static bool IsTokenType(Token *tok) {
    return IsTokenEqual(tok, "int") || IsTokenEqual(tok, "char");
}

//===================================================================
// declspec = "int"
// params  = ?declspec declarator ("," declspec declarator)* ")" 
// declarator = ("*")* ident type-suffix
// type-suffix = "(" param | "[" num "]
// declaration = declspec declarator ?("=" assign) (declarator ?("=" assign))* ";"
// compound_stmt = stmt* "}"
// stmt       = expr_stmt || "return" expr ";" || "if" "(" expr ")" stmt ("else" stmt)? || 
//              "for" "(" expr? ";" expr? ";" expr ")" stmt || "while" "(" expr ")" stmt ||
//              "{" compound_stmt ||
// expr_stmt  = expr ";"
// expr       = assign 
// assign     = equality ("=" assign)?
// equality   = relational ("==" relational | "!=" relational)*
// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
// mul = unary ("*" unary | "/" unary)* 
// postfix    = primary ("[" expr "]")*
// unary   = ("+" | "-" | "&" | "*") unary | postfix
// primary = "(" expr ")" | num | "sizeof" unary
//===================================================================
static Node *primary(Token **rest, Token *tok);
static Node *postfix(Token **rest, Token *tok);
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

static Type *params(Token **rest, Token *tok, Type *ty);
static Type *declspec(Token **rest, Token *tok);
static Type *type_suffix(Token **rest, Token *tok, Type *ty);
static Type *declarator(Token **rest, Token *tok, Type *ty);
static Node *declaration(Token **rest, Token *tok);
static void create_param_lvars(Type *param);
//===================================================================

static Type *declspec(Token **rest, Token *tok) {
    if (IsTokenEqual(tok, "char")) {
        *rest = SkipToken(tok, "char");
        return ty_char;
    }
    *rest = SkipToken(tok, "int");
    return ty_int;
}

static Type *params(Token **rest, Token *tok, Type *ty) {
    Type head = {};
    Type *cur = &head;

    while (!IsTokenEqual(tok, ")")) {
        if (cur != &head)
            tok = SkipToken(tok, ",");
        Type *basety = declspec(&tok, tok);
        Type *ty = declarator(&tok, tok, basety);
        cur = cur->next = CopyType(ty);
    }

    ty = NewTypeFn(ty);
    ty->params = head.next;
    *rest = tok->next;
    return ty;
}

static Type *type_suffix(Token **rest, Token *tok, Type *ty) {
    if (IsTokenEqual(tok, "("))
        return params(rest, tok->next, ty);
    
    if (IsTokenEqual(tok, "[")) {
        int len = GetTokenNum(tok->next);
        tok = SkipToken(tok->next->next, "]");
        ty = type_suffix(rest, tok, ty);
        return NewTypeArrayOf(ty, len);
    }
    *rest = tok;
    return ty;
}

static Type *declarator(Token **rest, Token *tok, Type *ty) {
    while (ConsumeToken(&tok, tok, "*"))
        ty = NewTypePTR2(ty);
    
    if (tok->kind != TK_IDENT)
        Error("expected a variable name");

    ty = type_suffix(rest, tok->next, ty);
    ty->name = tok;
    return ty;
}

static Node *declaration(Token **rest, Token *tok) {
    Type *base_type = declspec(&tok, tok);
    Node head = {};
    Node *cur = &head;
    int i = 0;

    while (!IsTokenEqual(tok, ";")) {
        if (i++ > 0)
            tok = SkipToken(tok, ",");

        Type *ty = declarator(&tok, tok, base_type);
        Obj *var = NewObjLVar(GetTokenIdent(ty->name), ty);

        if (!IsTokenEqual(tok, "="))
            continue;

        Node *lhs = NewNodeVar(var);
        Node *rhs = assign(&tok, tok->next);
        Node *node = NewNodeBinary(ND_ASSIGN, lhs, rhs);
        cur = cur->next = NewNodeUnary(ND_EXPR_STMT, node);
    }

    Node *node = NewNodeKind(ND_BLOCK);
    node->body = head.next;
    *rest = tok->next;
    return node;
}

static void create_param_lvars(Type *param) {
    if (param) {
        create_param_lvars(param->next);
        NewObjLVar(GetTokenIdent(param->name), param);
    }
}

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

        if (!ConsumeToken(&tok, tok, ";")) {
            node->init = expr(&tok, tok);
            tok = SkipToken(tok, ";");
        }
        if (!ConsumeToken(&tok, tok, ";")) {
            node->cond = expr(&tok, tok);
            tok = SkipToken(tok, ";");
        }
        if (!ConsumeToken(&tok, tok, ")")) {
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
        if (IsTokenType(tok))
            cur = cur->next = declaration(&tok, tok);
        else
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

    rhs = NewNodeBinary(ND_MUL, rhs, NewNodeNum(lhs->type->base->size));
    return NewNodeBinary(ND_ADD, lhs, rhs);
}

static Node *NewNodeSub(Node *lhs, Node *rhs) {
    AddType(lhs);
    AddType(rhs);

    if (IsTypeInteger(lhs->type) && IsTypeInteger(rhs->type))
        return NewNodeBinary(ND_SUB, lhs, rhs);

    if (lhs->type->base && rhs->type->base) {
        Node *node = NewNodeBinary(ND_SUB, lhs, rhs);
        node->type = ty_int;
        return NewNodeBinary(ND_DIV, node, NewNodeNum(lhs->type->base->size)); 
    }
    
    if (lhs->type->base && IsTypeInteger(rhs->type)) {
        rhs = NewNodeBinary(ND_MUL, rhs, NewNodeNum(lhs->type->base->size));
        AddType(rhs);
        Node *node = NewNodeBinary(ND_SUB, lhs, rhs);
        node->type = lhs->type;
        return node;
    }
    Error("invalid operands");
}

static Node *add(Token **rest, Token *tok) {
    Node *node = mul(&tok, tok);

    for (;;) {
        if (IsTokenEqual(tok, "+")) {
            node = NewNodeAdd(node, mul(&tok, tok->next));
            continue;
        }
        if (IsTokenEqual(tok, "-")) {
            node = NewNodeSub(node, mul(&tok, tok->next));
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
    return postfix(rest, tok);
}

static Node *postfix(Token **rest, Token *tok) {
    Node *node = primary(&tok, tok);

    while (IsTokenEqual(tok, "[")) {
        Node *index = expr(&tok, tok->next);
        tok = SkipToken(tok, "]");
        node = NewNodeUnary(ND_DEREF, NewNodeAdd(node, index));
    }

    *rest = tok;
    return node;
}

static Node *fncall(Token **rest, Token *tok) {
    Node *node = NewNodeKind(ND_FNCALL);
    node->fn_name = strndup(tok->str, tok->len);
    tok = tok->next->next;

    Node head = {};
    Node *cur = &head;

    while (!IsTokenEqual(tok, ")")) {
        cur = cur->next = expr(&tok, tok);
        if (!IsTokenEqual(tok, ")"))
            tok = SkipToken(tok, ",");
    }

    *rest = SkipToken(tok, ")");
    node->args = head.next;
    return node;
}

static Node *primary(Token **rest, Token *tok) {
    if (IsTokenEqual(tok, "(")) {
        Node *node = expr(&tok, tok->next);
        *rest = SkipToken(tok, ")");
        return node;
    }
    if (IsTokenEqual(tok, "sizeof")) {
        Node *node = unary(rest, tok->next);
        AddType(node);
        return NewNodeNum(node->type->size);
    }

    if (tok->kind == TK_IDENT) {
        if (IsTokenEqual(tok->next, "(")) {
            return fncall(rest, tok);
        } else {
            Obj *var = FindObjVar(tok);
            if (!var)
                Error("unexpected expression");
            *rest = tok->next;
            return NewNodeVar(var);
        }
    }

    if (tok->kind == TK_NUM) {
        Node *node = NewNodeNum(tok->val);
        *rest = tok->next;
        return node;
    }
    Error("Something is wrong");
}

static Token *Function(Token *tok, Type *base) {
    Type *ty = declarator(&tok, tok, base);
    
    Obj *fn = NewObjGVar(GetTokenIdent(ty->name), ty);
    fn->is_func = true;

    locals = NULL;
    create_param_lvars(ty->params);
    fn->params = locals;

    tok = SkipToken(tok, "{");
    fn->body = compound_stmt(&tok, tok);
    fn->locals = locals;
    
    return tok;
}

static Token *Gvar(Token *tok, Type *base) {
    for (int i = 0; !ConsumeToken(&tok, tok, ";"); i++) {
        if (i > 0) tok = SkipToken(tok, ",");

        Type *ty = declarator(&tok, tok, base);
        NewObjGVar(GetTokenIdent(ty->name), ty);
    }
    return tok;
}

static bool IsFunc(Token *tok) {
    if (IsTokenEqual(tok, ";")) return false;

    Type tmp = {};
    Type *ty = declarator(&tok, tok, &tmp);
    return ty->kind == TY_FN;
}

Obj *ParseToken(Token *tok) {
    globals = NULL;

    while (!IsTokenAtEof(tok)) {
        Type *base = declspec(&tok, tok);
        if (IsFunc(tok)) {
            tok = Function(tok, base);
            continue;
        }
        tok = Gvar(tok, base);
    }
    return globals;
}