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
};
Token *token;

Token *NewToken(Token *cur, TokenKind TK, char *str) {
    Token *new = calloc(sizeof(Token), 1);
    new->kind = TK;
    new->str = str;
    cur->next = new;
    return new;
}
bool ConsumeToken(char op) {
    if (token->kind != TK_RESERVED || token->str[0] != op) {
        return false;
    }
    token = token->next;
    return true;
}
void ExpectToken(char op) {
    if (!ConsumeToken(op)) {
         Error("It's not '%c'", op);
    }
}

int ExpectTokenNum() {
    if (token->kind != TK_NUM)
        Error("数ではありません");
    int val = token->val;
    token = token->next;
    return val;
}

bool IsTokenAtEof() {
  return token->kind == TK_EOF;
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
            cur = NewToken(cur, TK_RESERVED, p);
            p++;
            continue;
        }
        if (isdigit(*p)) {
            cur = NewToken(cur, TK_NUM, p);
            cur->val = strtol(p, &p, 10);
            continue;
        }
        Error("Can't tokenize!");
    }

    NewToken(cur, TK_EOF, p);
    return head.next;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        Error("引数の個数が正しくありません");
        return 1;
    }
    token = tokenize(argv[1]);

    println(".intel_syntax noprefix");
    println(".globl main");
    println("main:");
    println("\tmov rax, %d", ExpectTokenNum());
    while (!IsTokenAtEof()) {
        if (ConsumeToken('+')) {
            println("\tadd rax, %d", ExpectTokenNum());
            continue;
        }
        if (ConsumeToken('-')) {
            println("\tsub rax, %d", ExpectTokenNum());
            continue;
        }
        Error("something is wrong");
    }
    println("\tret");
    
    return 0;
}