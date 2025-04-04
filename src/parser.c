#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "5cc.h"


//===================================================================
static bool IsTokenEqual(Token *tok, char *op) {
    return strlen(op) == tok->len && !strncmp(tok->loc, op, tok->len);
    // return tok->kind == TK_RESERVED && memcmp(tok->loc, op, tok->len) == 0 && op[tok->len] == '\0';
}

static Token *SkipToken(Token *tok, char *s) {
  if (!IsTokenEqual(tok, s))
    ErrorToken(tok, "expected '%s'", s);
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

static char *NewUniqueName(void) {
    static int count = 0;
    char *name = calloc(sizeof(char), 16);
    sprintf(name, ".L.L.%d", count++);
    return name;
}

static char *GetTokenIdent(Token *tok) {
    if (tok->kind != TK_IDENT)
        ErrorToken(tok, "This is not ident");
    return strndup(tok->loc, tok->len);
}

static int GetTokenNum(Token *tok) {
    if (tok->kind != TK_NUM)
        ErrorToken(tok, "This is not number");
    return tok->val;
}

//===================================================================
typedef struct VarScope VarScope;
typedef struct TagScope TagScope;
typedef struct Scope Scope;

struct VarScope {
    VarScope *next;
    Obj *var;
    char *name;
    Type *type_def;
};

struct TagScope {
    TagScope *next;
    char *name;
    Type *type;
};

struct Scope {
    Scope *next;
    VarScope *vars;
    TagScope *tags;
};

typedef struct {
    bool is_typedef;
} VarAttr;

static Scope *scope = &(Scope){};

static void EnterScope() {
    Scope *new = calloc(1, sizeof(Scope));
    new->next = scope;
    scope = new;
}

static void LeaveScope() {
    scope = scope->next;
}

static VarScope *PushScope(char *name) {
    VarScope *new = calloc(1, sizeof(VarScope));
    new->name = name;
    new->next = scope->vars;
    scope->vars = new;
    return new;
}

static void PushTagScope(char *name, Type *type) {
    TagScope *new = calloc(1, sizeof(TagScope));
    new->name = name;
    new->type = type;
    new->next = scope->tags;
    scope->tags = new;
}

static Type *FindTagScope(Token *tok) {
    for (Scope *sc = scope; sc; sc = sc->next)
        for (TagScope *tsc = sc->tags; tsc; tsc = tsc->next)
            if (IsTokenEqual(tok, tsc->name))
                return tsc->type;
    return NULL;
}

static VarScope *FindVarScope(Token *tok) {
    for (Scope *sc = scope; sc; sc = sc->next)
        for (VarScope *vsc = sc->vars; vsc; vsc = vsc->next)
            if (IsTokenEqual(tok, vsc->name))
                return vsc;
    return NULL;
}

static Type *FindTypedef(Token *tok) {
    if (tok->kind == TK_IDENT) {
        VarScope *sc = FindVarScope(tok);
        if (sc)
            return sc->type_def;
    }
    return NULL;
}

static bool IsTokenType(Token *tok) {
    char *TY[] = {"int", "char", "long", "short", "struct", "union", "void", "typedef", NULL};
    for (int i = 0; TY[i]; i++)
        if (IsTokenEqual(tok, TY[i]))
            return true;
    return FindTypedef(tok);
}

//===================================================================
static Obj *locals;
static Obj *globals;

static Obj *NewObj(char *name, Type *type) {
    Obj *new = calloc(1, sizeof(Obj));
    new->name = name;
    new->type = type;
    return new;
}

static Obj *NewObjMember(char *name, Type *type) {
    Obj *new = NewObj(name, type);
    new->is_member = true;
    return new;
}

static Obj *NewObjVar(char *name, Type *type) {
    Obj *new = NewObj(name, type);
    PushScope(name)->var = new;
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
    new->next = globals;
    globals = new;
    return new;
}

static Obj *FindObjMember(Type *type, Token *tok) {
    for (Obj *mem = type->members; mem; mem = mem->next)
        if (IsTokenEqual(tok, mem->name))
            return mem;
    ErrorToken(tok, "No such member");
}

static Obj *NewObjGVarAnon(Type *type) {
    return NewObjGVar(NewUniqueName(), type);
}

static Obj *NewObjString(Token *tok) {
    Obj *new = NewObjGVarAnon(NewTypeArrayOf(ty_char, strlen(tok->string) + 1));
    new->init_data = tok->string;
    return new;
}

//===================================================================
static Node *NewNodeKind(NodeKind kind, Token *tok) {
    Node *new = calloc(1, sizeof(Node));
    new->kind = kind;
    new->tok = tok;
    return new;
}

static Node *NewNodeBinary(NodeKind kind, Token *tok, Node *lhs, Node *rhs) {
    Node *new = NewNodeKind(kind, tok);
    new->lhs = lhs;
    new->rhs = rhs;
    return new;
}

static Node *NewNodeNum(Token *tok, int64_t val) {
    Node *new = NewNodeKind(ND_NUM, tok);
    new->val = val;
    return new;
}

static Node *NewNodeUnary(NodeKind kind, Token *tok, Node *lhs) {
    Node *new = NewNodeKind(kind, tok);
    new->lhs = lhs;
    return new;
}

static Node *NewNodeVar(Token *tok, Obj *var) {
    Node *new = NewNodeKind(ND_VAR, tok);
    new->var = var;
    new->type = var->type;
    return new;
}

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
static Type *declspec(Token **rest, Token *tok, VarAttr *attr);
static Type *type_suffix(Token **rest, Token *tok, Type *ty);
static Type *declarator(Token **rest, Token *tok, Type *ty);
static Node *declaration(Token **rest, Token *tok, Type *base);
static void create_param_lvars(Type *param);
static Type *struct_declspec(Token **rest, Token *tok);
static void struct_members(Token **rest, Token *tok, Type *type);
static Type *union_declspec(Token **rest, Token *tok);
static void parse_typedef(Token **rest, Token *tok, Type *base);
//===================================================================

static Type *declspec(Token **rest, Token *tok, VarAttr *attr) {
    enum {
        VOID  = 1 << 0,
        CHAR  = 1 << 2,
        SHORT = 1 << 4,
        INT   = 1 << 6,
        LONG  = 1 << 8,
        OTHER = 1 << 10,
    };

    Type *ty = ty_int;
    int counter = 0;

    while (IsTokenType(tok)) {
        if (IsTokenEqual(tok, "typedef")) {
            if (!attr)
                ErrorToken(tok, "storage class specifier is not allowed in this context");
            attr->is_typedef = true;
            tok = tok->next;
            continue;
        }
        Type *ty2 = FindTypedef(tok);
        if (IsTokenEqual(tok, "struct") || IsTokenEqual(tok, "union") || ty2) {
            if (counter)
                break;
            if (IsTokenEqual(tok, "struct")) ty = struct_declspec(&tok, tok->next);
            else if (IsTokenEqual(tok, "union")) ty = union_declspec(&tok, tok->next);
            else {
                ty = ty2;
                tok = tok->next;
            }
            counter += OTHER;
            continue;
        }

        if (IsTokenEqual(tok, "int")) counter += INT;
        else if (IsTokenEqual(tok, "char")) counter += CHAR;
        else if (IsTokenEqual(tok, "long")) counter += LONG;
        else if (IsTokenEqual(tok, "short")) counter += SHORT;
        else if (IsTokenEqual(tok, "void")) counter += VOID;

        switch (counter) {
        case VOID:
            ty = ty_void;
            break;
        case CHAR:
            ty = ty_char;
            break;
        case SHORT:
        case SHORT + INT:
            ty = ty_short;
            break;
        case INT:
            ty = ty_int;
            break;
        case LONG:
        case LONG + INT:
        case LONG + LONG:
        case LONG + LONG + INT:
            ty = ty_long;
            break;
        default:
            ErrorToken(tok, "invalid type");
        }
        tok = tok->next;
    }
    *rest = tok;
    return ty;
}

static Type *struion_declspec(Token **rest, Token *tok) {
    Token *tag = NULL;
    if (tok->kind == TK_IDENT) {
        tag = tok;
        tok = tok->next;
    }

    if (tag && !IsTokenEqual(tok, "{")) {
        Type *type = FindTagScope(tag);
        if (!type) ErrorToken(tok, "undefined struct");
        *rest = tok;
        return type;
    }

    Type *type = calloc(1, sizeof(Type));
    type->kind = TY_STRUCT;
    struct_members(rest, tok->next, type);
    type->align = 1;

    if (tag)
        PushTagScope(GetTokenIdent(tag), type);
    return type;
}

static Type *struct_declspec(Token **rest, Token *tok) {
    Type *type = struion_declspec(rest, tok);
    type->kind = TY_STRUCT;

    int offset = 0;
    for (Obj *mem = type->members; mem; mem = mem->next) {
        offset = align_to(offset, mem->type->align);
        mem->offset = offset;
        offset += mem->type->size;
        if (type->align < mem->type->align)
            type->align = mem->type->align;
    }
    type->size = align_to(offset, type->align);
    
    return type;
}

static Type *union_declspec(Token **rest, Token *tok) {
    Type *type = struion_declspec(rest, tok);
    type->kind = TY_UNION;
    
    for (Obj *mem = type->members; mem; mem = mem->next) {
        if (type->align < mem->type->align)
            type->align = mem->type->align;
        if (type->size < mem->type->size)
            type->size = mem->type->size;
    }
    type->size = align_to(type->size, type->align);
    return type;
}

static void struct_members(Token **rest, Token *tok, Type *type) {
    Obj head = {};
    Obj *cur = &head;
    while (!IsTokenEqual(tok, "}")) {
        Type *base = declspec(&tok, tok, NULL);

        for (int i = 0; !ConsumeToken(&tok, tok, ";"); i++) {
            if (i > 0)
                tok = SkipToken(tok, ",");

            Type *ty = declarator(&tok, tok, base);
            cur = cur->next = NewObjMember(GetTokenIdent(ty->name), ty);
        }
    }
    *rest = tok->next;
    type->members = head.next;
}

static Type *params(Token **rest, Token *tok, Type *ty) {
    Type head = {};
    Type *cur = &head;

    while (!IsTokenEqual(tok, ")")) {
        if (cur != &head)
            tok = SkipToken(tok, ",");
        Type *basety = declspec(&tok, tok, NULL);
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

     if (IsTokenEqual(tok, "(")) {
        Token *start = tok;
        Type dummy = {};
        declarator(&tok, start->next, &dummy);
        tok = SkipToken(tok, ")");
        ty = type_suffix(rest, tok, ty);
        return declarator(&tok, start->next, ty);
    }
    
    if (tok->kind != TK_IDENT)
        ErrorToken(tok, "expected a variable name");

    

    ty = type_suffix(rest, tok->next, ty);
    ty->name = tok;

    return ty;
}

static Node *declaration(Token **rest, Token *tok, Type *base_type) {
    Node head = {};
    Node *cur = &head;

    for (int i = 0; !IsTokenEqual(tok, ";"); i++) {
        if (i > 0)
            tok = SkipToken(tok, ",");

        Type *ty = declarator(&tok, tok, base_type);
        if (ty->kind == TY_VOID) ErrorToken(tok, "variable declared void");
        Obj *var = NewObjLVar(GetTokenIdent(ty->name), ty);

        if (!IsTokenEqual(tok, "="))
            continue;

        Node *lhs = NewNodeVar(tok, var);
        Node *rhs = assign(&tok, tok->next);
        Node *node = NewNodeBinary(ND_ASSIGN, tok, lhs, rhs);
        cur = cur->next = NewNodeUnary(ND_EXPR_STMT, tok, node);
    }

    Node *node = NewNodeKind(ND_BLOCK, tok);
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

static void parse_typedef(Token **rest, Token *tok, Type *base) {
    for (int i = 0; !ConsumeToken(&tok, tok, ";"); i++) {
        if (i > 0)
            tok = SkipToken(tok, ",");
        Type *ty = declarator(&tok, tok, base);
        PushScope(GetTokenIdent(ty->name))->type_def = ty;
    }
    *rest = tok;
}

static Type *abstract_declarator(Token **rest, Token *tok, Type *ty) {
    while (ConsumeToken(&tok, tok, "*"))
        ty = NewTypePTR2(ty);

     if (IsTokenEqual(tok, "(")) {
        Token *start = tok;
        Type dummy = {};
        abstract_declarator(&tok, start->next, &dummy);
        tok = SkipToken(tok, ")");
        ty = type_suffix(rest, tok, ty);
        return abstract_declarator(&tok, start->next, ty);
    }
    
    return type_suffix(rest, tok, ty);
}

static Type *type_name(Token **rest, Token *tok) {
    Type *ty = declspec(&tok, tok, NULL);
    return abstract_declarator(rest, tok, ty);
}

static Node *stmt(Token **rest, Token *tok) {
    if (IsTokenEqual(tok, "return")) {
        Node *node = NewNodeUnary(ND_RETURN, tok, expr(&tok, tok->next));
        *rest = SkipToken(tok, ";");
        return node;
    }
    if (IsTokenEqual(tok, "{")) {
        return compound_stmt(rest, tok->next);
    }
    if (IsTokenEqual(tok, "if")) {
        Node *node = NewNodeKind(ND_IF, tok);
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
        Node *node = NewNodeKind(ND_FOR, tok);
        tok = SkipToken(tok->next, "(");

        EnterScope();
        if (!ConsumeToken(&tok, tok, ";")) {
            if (IsTokenType(tok)) {
                Type *base = declspec(&tok, tok, NULL);
                node->init = declaration(&tok, tok, base);
            } else
                node->init = expr_stmt(&tok, tok);
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
        LeaveScope();
        *rest = tok;
        return node;
    }
    if (IsTokenEqual(tok, "while")) {
        Node *node = NewNodeKind(ND_FOR, tok);
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
    EnterScope();
    while (!IsTokenEqual(tok, "}")) {
        if (IsTokenType(tok)) {
            VarAttr attr = {};
            Type *base = declspec(&tok, tok, &attr);

            if (attr.is_typedef) {
                parse_typedef(&tok, tok, base);
                continue;
            }

            cur = cur->next = declaration(&tok, tok, base);
        } else {
            cur = cur->next = stmt(&tok, tok);
        }
        AddType(cur);
    }

    LeaveScope();
    Node *node = NewNodeKind(ND_BLOCK, tok);
    node->body = head.next;
    *rest = tok->next;
    return node;
}

static Node *expr_stmt(Token **rest, Token *tok) {
    if (IsTokenEqual(tok, ";")) {
        *rest = SkipToken(tok, ";");
        return NewNodeKind(ND_BLOCK, tok);
    }
    Node *node = NewNodeUnary(ND_EXPR_STMT, tok, expr(&tok, tok));
   *rest = SkipToken(tok, ";");
    return node;
}

static Node *expr(Token **rest, Token *tok) {
    Node *node = assign(&tok, tok);
    if (IsTokenEqual(tok, ",")) {
        return NewNodeBinary(ND_COMMA, tok, node, expr(rest, tok->next));
    }
    *rest = tok;
    return node;
}

static Node *assign(Token **rest, Token *tok) {
    Node *node = equality(&tok, tok);

    if (IsTokenEqual(tok, "=")) {
        node = NewNodeBinary(ND_ASSIGN, tok, node, assign(&tok, tok->next));
    }
    *rest = tok;
    return node;
}

static Node *equality(Token **rest, Token *tok) {
    Node *node = relational(&tok, tok);

    for (;;) {
        if (IsTokenEqual(tok, "==")) {
            node = NewNodeBinary(ND_EQ, tok, node, add(&tok, tok->next));
            continue;
        }
        if (IsTokenEqual(tok, "!=")) {
            node = NewNodeBinary(ND_NE, tok, node, add(&tok, tok->next));
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
            node = NewNodeBinary(ND_LE, tok, node, add(&tok, tok->next));
            continue;
        }
        if (IsTokenEqual(tok, "<")) {
            node = NewNodeBinary(ND_LT, tok, node, add(&tok, tok->next));
            continue;
        }
        if (IsTokenEqual(tok, ">=")) {
            node = NewNodeBinary(ND_LE, tok, add(&tok, tok->next), node);
            continue;
        }
        if (IsTokenEqual(tok, ">")) {
            node = NewNodeBinary(ND_LT, tok, add(&tok, tok->next), node);
            continue;
        }
        *rest = tok;
        return node;
    }
}

static Node *NewNodeAdd(Token *tok, Node *lhs, Node *rhs) {
    AddType(lhs);
    AddType(rhs);

    if (IsTypeInteger(lhs->type) && IsTypeInteger(rhs->type))
        return NewNodeBinary(ND_ADD, tok, lhs, rhs);

    if (lhs->type->base && rhs->type->base)
        ErrorToken(tok, "can't add ptr to ptr.");
    
    if (!lhs->type->base && rhs->type->base)
        return NewNodeAdd(tok, rhs, lhs);

    rhs = NewNodeBinary(ND_MUL, tok, rhs, NewNodeNum(tok, lhs->type->base->size));
    return NewNodeBinary(ND_ADD, tok, lhs, rhs);
}

static Node *NewNodeSub(Token *tok, Node *lhs, Node *rhs) {
    AddType(lhs);
    AddType(rhs);

    if (IsTypeInteger(lhs->type) && IsTypeInteger(rhs->type))
        return NewNodeBinary(ND_SUB, tok, lhs, rhs);

    if (lhs->type->base && rhs->type->base) {
        Node *node = NewNodeBinary(ND_SUB, tok, lhs, rhs);
        node->type = ty_int;
        return NewNodeBinary(ND_DIV, tok, node, NewNodeNum(tok, lhs->type->base->size)); 
    }
    
    if (lhs->type->base && IsTypeInteger(rhs->type)) {
        rhs = NewNodeBinary(ND_MUL, tok, rhs, NewNodeNum(tok, lhs->type->base->size));
        AddType(rhs);
        Node *node = NewNodeBinary(ND_SUB, tok, lhs, rhs);
        node->type = lhs->type;
        return node;
    }
    ErrorToken(tok ,"invalid operands");
}

static Node *add(Token **rest, Token *tok) {
    Node *node = mul(&tok, tok);

    for (;;) {
        if (IsTokenEqual(tok, "+")) {
            node = NewNodeAdd(tok, node, mul(&tok, tok->next));
            continue;
        }
        if (IsTokenEqual(tok, "-")) {
            node = NewNodeSub(tok, node, mul(&tok, tok->next));
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
            node = NewNodeBinary(ND_MUL, tok, node, unary(&tok, tok->next));
            continue;
        }
        if (IsTokenEqual(tok, "/")) {
            node = NewNodeBinary(ND_DIV, tok, node, unary(&tok, tok->next));
            continue;
        }
        if (IsTokenEqual(tok, "%")) {
            node = NewNodeBinary(ND_MOD, tok, node, unary(&tok, tok->next));
            continue;
        }
        if (IsTokenEqual(tok, "&")) {
            node = NewNodeBinary(ND_AND, tok, node, unary(&tok, tok->next));
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
        return NewNodeUnary(ND_NEG, tok, unary(rest, tok->next));
    }
    if (IsTokenEqual(tok, "*")) {
        return NewNodeUnary(ND_DEREF, tok, unary(rest, tok->next));
    }
    if (IsTokenEqual(tok, "&")) {
        return NewNodeUnary(ND_ADDR, tok, unary(rest, tok->next));
    }
    return postfix(rest, tok);
}

static Node *struct_ref(Token *tok, Node *lhs) {
    AddType(lhs);
    if (lhs->type->kind != TY_STRUCT && lhs->type->kind != TY_UNION) ErrorToken(lhs->tok, "not a struct nor an union");
    Node *node = NewNodeUnary(ND_DOTS, tok, lhs);
    node->member = FindObjMember(lhs->type, tok->next);
    return node;
}

static Node *postfix(Token **rest, Token *tok) {
    Node *node = primary(&tok, tok);
    for (;;) {
        if (IsTokenEqual(tok, "[")) { // a[b] => *(a + b)
            Node *index = expr(&tok, tok->next);
            tok = SkipToken(tok, "]");
            node = NewNodeUnary(ND_DEREF, tok, NewNodeAdd(tok, node, index));
            continue;
        }
        if (IsTokenEqual(tok, ".")) {
            node = struct_ref(tok, node);
            tok = tok->next->next;
            continue;
        }
        if (IsTokenEqual(tok, "->")) { // a->b => (*a).b
            node = NewNodeUnary(ND_DEREF, tok, node);
            node = struct_ref(tok, node);
            tok = tok->next->next;
            continue;
        }
        *rest = tok;
        return node;
    } 
}

static Node *fncall(Token **rest, Token *tok) {
    Node *node = NewNodeKind(ND_FNCALL, tok);
    node->fn_name = strndup(tok->loc, tok->len);
    tok = tok->next->next;

    Node head = {};
    Node *cur = &head;

    while (!IsTokenEqual(tok, ")")) {
        cur = cur->next = assign(&tok, tok);
        if (!IsTokenEqual(tok, ")"))
            tok = SkipToken(tok, ",");
    }

    *rest = SkipToken(tok, ")");
    node->args = head.next;
    return node;
}

static Node *primary(Token **rest, Token *tok) {
    if (IsTokenEqual(tok, "(")) {
        if (IsTokenEqual(tok->next, "{")) {
            Node *node = NewNodeKind(ND_STMT_EXPR, tok);
            node->body =  compound_stmt(&tok, tok->next->next)->body;
            *rest = SkipToken(tok, ")");
            return node;
        } else {
            Node *node = expr(&tok, tok->next);
            *rest = SkipToken(tok, ")");
            return node;
        }
    }
    if (IsTokenEqual(tok, "sizeof")) {
        Token *start = tok;
        if (IsTokenEqual(tok->next, "(") && IsTokenType(tok->next->next)) {
            Type *base = type_name(&tok, tok->next->next);
            *rest = SkipToken(tok, ")");
            
            return NewNodeNum(start, base->size);
        }
        Node *node = unary(rest, tok->next);
        AddType(node);
        return NewNodeNum(tok, node->type->size);
    }

    if (tok->kind == TK_IDENT) {
        if (IsTokenEqual(tok->next, "(")) {
            return fncall(rest, tok);
        } else {
            VarScope *sc = FindVarScope(tok);
            if (!sc || !sc->var)
                ErrorToken(tok, "undeclared valuable");
            *rest = tok->next;
            return NewNodeVar(tok, sc->var);
        }
    }

    if (tok->kind == TK_NUM) {
        Node *node = NewNodeNum(tok, tok->val);
        *rest = tok->next;
        return node;
    }

    if (tok->kind == TK_STR) {
        Obj *str = NewObjString(tok);
        *rest = tok->next;
        return NewNodeVar(tok, str);
    }
    ErrorToken(tok, "Something is wrong");
}

static Token *Function(Token *tok, Type *base) {
    Type *ty = declarator(&tok, tok, base);
    
    Obj *fn = NewObjGVar(GetTokenIdent(ty->name), ty);
    fn->is_func = true;
    fn->is_def = !ConsumeToken(&tok, tok, ";");

    if (!fn->is_def)
        return tok;

    locals = NULL;
    EnterScope();
    create_param_lvars(ty->params);
    fn->params = locals;

    tok = SkipToken(tok, "{");
    fn->body = compound_stmt(&tok, tok);
    fn->locals = locals;
    
    LeaveScope();
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
        VarAttr attr = {};
        Type *base = declspec(&tok, tok, &attr);

        if (attr.is_typedef) {
            parse_typedef(&tok, tok, base);
            continue;
        }
        if (IsFunc(tok)) {
            tok = Function(tok, base);
            continue;
        }
        tok = Gvar(tok, base);
    }
    return globals;
}