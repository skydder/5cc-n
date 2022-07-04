#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "5cc.h"

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

Token *NewToken(TokenKind TK, char *start, char *end) {
    Token *new = calloc(sizeof(Token), 1);
    new->kind = TK;
    new->str = start;
    new->len = end - start;
    return new;
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