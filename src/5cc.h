#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>


typedef enum {
    TK_RESERVED,
    TK_NUM,
    TK_INDENT,
    TK_EOF,
} TokenKind;

typedef struct Token Token;
struct Token {
    TokenKind kind;
    Token *next;
    int val;
    char *str;
    int len;
};

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
} NodeKind;
typedef struct Node Node;
typedef struct Obj Obj;
struct Obj {
    Obj *next;
    char *name;
    int offset;

    bool is_lvar;
    bool is_func;

    Obj *locals;
    Node *prog;
    int stack_size;
};

struct Node {
    NodeKind kind;
    Node *next;
    Node *lhs;
    Node *rhs;
    int val;
    Obj *var;
};



Token *Tokenize(char *p);
Obj *ParseToken(Token *tok);
void GenCode(Obj *func);

void Error(char *fmt, ...);
void println(char *fmt, ...);