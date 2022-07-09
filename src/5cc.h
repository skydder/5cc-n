#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>


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
struct Node {
    NodeKind kind;
    Node *next;
    Node *lhs;
    Node *rhs;
    int val;
    char name;
};

Token *Tokenize(char *p);
Node *ParseToken(Token *tok);
void GenCode(Node *node);

void Error(char *fmt, ...);
void println(char *fmt, ...);