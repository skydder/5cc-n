#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>


typedef enum {
    TK_RESERVED,
    TK_NUM,
    TK_IDENT,
    TK_EOF,
} TokenKind;

typedef enum {
    ND_ADD,
    ND_SUB,
    ND_MUL,
    ND_DIV,
    ND_NEG,
    ND_NUM,
    ND_EQ, // ==
    ND_NE, // !=
    ND_LT, // <
    ND_LE, // <=
    ND_ASSIGN,
    ND_VAR,
    ND_EXPR_STMT,
    ND_RETURN,
    ND_BLOCK,
    ND_IF,
    ND_FOR,
    ND_ADDR,
    ND_DEREF,
    ND_FNCALL,
} NodeKind;

typedef enum {
    TY_INT,
    TY_PTR,
    TY_FN,
    TY_ARRAY,
} TypeKind;

typedef struct Token Token;
typedef struct Node Node;
typedef struct Obj Obj;
typedef struct Type Type;

struct Token {
    TokenKind kind;
    Token *next;
    int val;
    char *str;
    int len;
};

struct Obj {
    Obj *next;
    char *name;
    Type *type;

    // for Lvar
    bool is_lvar;
    int offset;
    
    // for Fn
    bool is_func;
    Obj *locals;
    Obj *params;
    Node *body;
    int stack_size;
};

struct Node {
    NodeKind kind;
    Type *type;
    Node *next;
    Node *lhs;
    Node *rhs;

    Node *cond;
    Node *then;
    Node *_else;

    Node *init;
    Node *inc;

    int val;

    Obj *var;
    Node *body;
    char *fn_name;
    Node *args;
};

struct Type {
    TypeKind kind;
    Type *base;
    int size;
    
    int array_len;

    Token *name;
    Type *return_type;  // for func
    Type *params;
    Type *next;
};


Token *Tokenize(char *p);
Obj *ParseToken(Token *tok);
void GenCode(Obj *func);

void Error(char *fmt, ...);
void println(char *fmt, ...);

void AddType(Node *node);
bool IsTypeInteger(Type *ty);
Type *NewTypePTR2(Type *base);
Type *NewTypeFn(Type *return_type);
Type *NewTypeArrayOf(Type *base, int len);
Type *CopyType(Type *ty);
extern Type *ty_int;