#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

char *UserInput;

void Error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[ERROR] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}
void println(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    return ;
}

typedef enum {
    TK_RESERVED,
    TK_NUM,
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

Token *NewToken(TokenKind TK, char *start, char *end) {
    Token *new = calloc(sizeof(Token), 1);
    new->kind = TK;
    new->str = start;
    new->len = end - start;
    return new;
}

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
bool is_alnum(char c) {
    return ('a' <= c && c <= 'z') ||
           ('A' <= c && c <= 'Z') ||
           ('0' <= c && c <= '9') ||
           (c == '_');
}

bool IsStrSame(char *A, char *B) {
    return (strncmp(A, B, strlen(B)) == 0);
}

bool IsStrReserved(char *A, char *reserved) {
    return IsStrSame(A, reserved) && !is_alnum(A[strlen(reserved)]);
}

Token *NewTokenReserved(char **start) {
    Token *new = NULL;
    struct {
        char *word;
        int len;
    } symbol[] = {
        {"<=", 2}, {">=", 2}, {"==", 2}, {"!=", 2},
        {"-", 1}, {"+", 1}, {"/", 1}, {"*", 1},
        {"<", 1}, {">", 1}, {"(", 1}, {")", 1},
        {NULL, 0},
    };
    for (int i = 0; symbol[i].word; i++) {
        if (IsStrSame(*start, symbol[i].word)) {
            new = NewToken(TK_RESERVED, *start, *start + symbol[i].len);
            *start = *start + symbol[i].len;
            return new;
        }
    }
    return new;
}

Token *Tokenize(char *p) {
    Token head;
    head.next = NULL;
    Token *cur = &head;

    while (*p) {
        if (isspace(*p)) {
            p++;
            continue;
        }

        Token *tok = NewTokenReserved(&p);
        if (tok) {
            cur = cur->next = tok;
            continue;
        }

        if (isdigit(*p)) {
            cur = cur->next = NewToken(TK_NUM, p, p);
            char *q = p;
            cur->val = strtol(p, &p, 10);
            cur->len = p - q;
            continue;
        }
        Error("Can't tokenize!");
    }

    cur = cur->next = NewToken(TK_EOF, p, p);
    return head.next;
}

//===================================================================

typedef struct Node Node;
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
} NodeKind;
struct Node {
    NodeKind kind;
    Node *lhs;
    Node *rhs;
    int val;
};
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
Node *expr(Token **rest, Token *tok);
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

//===================================================================
static int depth;

static void push(void) {
  println("\tpush %%rax");
  depth++;
}

static void pop(char *arg) {
  println("\tpop %s", arg);
  depth--;
}

static void gen_expr(Node *node) {
    switch (node->kind) {
    case ND_NUM:
        println("\tmov $%d, %%rax", node->val);
        return;
    case ND_NEG:
        gen_expr(node->lhs);
        println("\tneg %%rax");
        return;
    }

    gen_expr(node->rhs);
    push();
    gen_expr(node->lhs);
    pop("%rdi");

    switch (node->kind) {
    case ND_ADD:
        println("\tadd %%rdi, %%rax");
        return;
    case ND_SUB:
        println("\tsub %%rdi, %%rax");
        return;
    case ND_MUL:
        println("\timul %%rdi, %%rax");
        return;
    case ND_DIV:
        println("\tcqo");
        println("\tidiv %%rdi");
        return;
    case ND_EQ:
    case ND_NE:
    case ND_LE:
    case ND_LT:
        println("\tcmp %%rdi, %%rax");
        if (node->kind == ND_EQ) {
            println("\tsete %%al");
        } else if (node->kind == ND_NE) {
            println("\tsetne %%al");
        } else if (node->kind == ND_LT) {
            println("\tsetl %%al");
        } else if (node->kind == ND_LE) {
            println("\tsetle %%al");
        }
        println("\tmovzb %%al, %%rax");
        return;
    }

    Error("invalid expression");
}
//===================================================================
int main(int argc, char **argv) {
    if (argc != 2) {
        Error("引数の個数が正しくありません");
        return 1;
    }
    UserInput = argv[1];

    Token *token = Tokenize(argv[1]);
    Node *node = expr(&token, token);

    if (token->kind != TK_EOF)
        Error("extra token");

    println(".globl main");
    println("main:");
    gen_expr(node);
    println("\tret");
    assert(depth == 0);

    return 0;
}