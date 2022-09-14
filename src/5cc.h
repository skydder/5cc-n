#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TK_RESERVED,
    TK_NUM,
    TK_IDENT,
    TK_STR,
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
    ND_COMMA,
    ND_DOTS,  // struct or union
    ND_STMT_EXPR,
} NodeKind;

typedef enum {
    TY_INT,
    TY_CHAR,
    TY_LONG,
    TY_SHORT,
    TY_PTR,
    TY_FN,
    TY_ARRAY,
    TY_STRUCT,
    TY_UNION
} TypeKind;

typedef struct Token Token;
typedef struct Node Node;
typedef struct Obj Obj;
typedef struct Type Type;

struct Token {
    TokenKind kind;
    Token *next;
    char *loc;
    int len;

    int64_t val;
    char *string;
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
    bool is_def;
    Obj *locals;
    Obj *params;
    Node *body;
    int stack_size;

    char *init_data;

    bool is_member;
};

struct Node {
    NodeKind kind;
    Type *type;
    Node *next;
    Token *tok;

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

    Obj *member;
};

struct Type {
    TypeKind kind;
    Type *base;
    int size;
    int align;
    
    int array_len;

    Obj *members;

    Token *name;
    Type *return_type;  // for func
    Type *params;
    Type *next;
};


Token *Tokenize(char *p);
Obj *ParseToken(Token *tok);
void GenCode(Obj *func);

void AddType(Node *node);
bool IsTypeInteger(Type *ty);
Type *NewTypePTR2(Type *base);
Type *NewTypeFn(Type *return_type);
Type *NewTypeArrayOf(Type *base, int len);
Type *CopyType(Type *ty);
extern Type *ty_int;
extern Type *ty_char;
extern Type *ty_long;
extern Type *ty_short;

extern char *UserInput;
extern char *FileName;
bool IsStrSame(char *A, char *B);
void println(char *fmt, ...);
void Error(char *fmt, ...);
void ErrorAt(char *loc, char *fmt, ...);
void ErrorToken(Token *tok, char *fmt, ...);
void Debug(char *fmt, ...);
void PrintToken(Token *tok);

int align_to(int n, int align);