#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

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
bool ConsumeToken(Token *tok, char op) {
    if (tok->kind != TK_RESERVED || tok->str[0] != op) {
        return false;
    }
    tok = tok->next;
    return true;
}
void ExpectToken(Token *tok,char op) {
    if (!ConsumeToken(tok, op)) {
         Error("It's not '%c'", op);
    }
}
bool IsTokenEqual(Token *tok, char *op) {
  return memcmp(tok->str, op, tok->len) == 0 && op[tok->len] == '\0';
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

Token *tokenize(char *p) {
    Token head;
    head.next = NULL;
    Token *cur = &head;

    while (*p) {
        if (isspace(*p)) {
            p++;
            continue;
        }
        if (*p =='+' || *p == '-') {
            cur = cur->next = NewToken(TK_RESERVED, p, p + 1);
            p++;
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

int main(int argc, char **argv) {
    if (argc != 2) {
        Error("引数の個数が正しくありません");
        return 1;
    }
    Token *token = tokenize(argv[1]);

    println(".intel_syntax noprefix");
    println(".globl main");
    println("main:");
    println("\tmov rax, %d", ExpectTokenNum(&token));
    //token = token->next;

    while (!IsTokenAtEof(token)) {
        if (IsTokenEqual(token, "+")) {
            token = token->next;
            println("\tadd rax, %d", ExpectTokenNum(&token));
            continue;
        }
        if (IsTokenEqual(token, "+")) {
            token = token->next;
            println("\tsub rax, %d", ExpectTokenNum(&token));
            continue;
        }

        Error("something is wrong");
    }
    println("\tret");
    
    return 0;
}