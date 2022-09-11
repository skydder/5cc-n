#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "5cc.h"

static bool is_al(char c) {
    return isalpha(c) ||
           (c == '_');
}

static bool is_alnum(char c) {
    return is_al(c) || ('0' <= c && c <= '9');
}

static bool IsStrReserved(char *A, char *reserved) {
    return IsStrSame(A, reserved) && !is_alnum(A[strlen(reserved)]);
}

static Token *NewToken(TokenKind TK, char *start, char *end) {
    Token *new = calloc(1, sizeof(Token));
    new->kind = TK;
    new->loc = start;
    new->len = end - start;
    return new;
}

static Token *NewTokenReserved(char **start) {
    Token *new = NULL;
    static struct {
        char *word;
        int len;
    } symbol[] = {
        {"<=", 2}, {">=", 2}, {"==", 2}, {"!=", 2}, {"->", 2}, 
        {"-", 1}, {"+", 1}, {"/", 1}, {"*", 1},
        {"<", 1}, {">", 1}, {"(", 1}, {")", 1},
        {";", 1}, {"=", 1}, {"{", 1}, {"}", 1},
        {"&", 1}, {",", 1}, {"[", 1}, {"]", 1},
        {".", 1},
        {NULL, 0},
    };

    static struct {
        char *word;
        int len;
    } keyword[] = {
        {"return", 6}, {"for", 3}, {"else", 4}, {"if", 2},
        {"sizeof", 6}, {"int", 3}, {"char", 4}, {"while", 5},
        {"struct", 6},
        {NULL, 0},
    };

    for (int i = 0; symbol[i].word; i++) {
        if (IsStrSame(*start, symbol[i].word)) {
            new = NewToken(TK_RESERVED, *start, *start + symbol[i].len);
            *start = *start + symbol[i].len;
            return new;
        }
    }
    for (int i = 0; keyword[i].word; i++) {
        if (IsStrReserved(*start, keyword[i].word)) {
            new = NewToken(TK_RESERVED, *start, *start + keyword[i].len);
            *start = *start + keyword[i].len;
            return new;
        }
    }
    return new;
}

static char ReadEscapedLiteral(char *p) {
    switch (*p) {
    case 'a': return '\a';
    case 'b': return '\b';
    case 't': return '\t';
    case 'n': return '\n';
    case 'v': return '\v';
    case 'f': return '\f';
    case 'r': return '\r';
    case 'e': return 27;
    default: return *p;
    }
}

static char *EndOfStrLiteral(char *p) {
    char *start = p;
    for (; *p != '\"'; p++) {
        if (*p == '\n' || *p == '\0') {
            ErrorAt(start, "unclosed string literal");
        }
        if (*p == '\\') p++;
    }
    return p;
}

static Token *ReadStrLiteral(char **start) {
    char *end = EndOfStrLiteral(*start + 1);
    char *string = calloc(end - *start, sizeof(char));
    int len  = 0;

    for (char *p = *start + 1; p < end; p++) {
        if (*p == '\\') {
            string[len++] = ReadEscapedLiteral(p + 1);
            p++;
        } else { 
            string[len++] = *p;
        }
    }
    Token *tok = NewToken(TK_STR, *start, end + 1);
    tok->string = string;
    *start = end + 1;
    return tok;
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
        
        if (isdigit(*p)) {
            cur = cur->next = NewToken(TK_NUM, p, p);
            char *q = p;
            cur->val = strtol(p, &p, 10);
            cur->len = p - q;
            continue;
        }

        if (IsStrSame(p, "//")) {
            p += 2;
            while (*p != '\n') p++;
            continue;
        }
        
        if (IsStrSame(p, "/*")) {
            char *q = strstr(p + 2, "*/");
            if (!q) ErrorAt(p, "unclosed comment");
            p = q + 2;
            continue;
        }

        Token *tok = NewTokenReserved(&p);
        if (tok) {
            cur = cur->next = tok;
            continue;
        }

        if (*p == '"') {
            cur = cur->next = ReadStrLiteral(&p);
            continue;
        }

        if (is_al(*p)) {
            char *start = p;
            for (; is_alnum(*p);) p++;  // len(ident_name)
            cur = cur->next = NewToken(TK_IDENT, start, p);
            continue;
        }

         ErrorAt(p, "Can't tokenize!");
    }

    cur = cur->next = NewToken(TK_EOF, p, p);
    return head.next;
}