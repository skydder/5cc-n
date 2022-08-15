#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "5cc.h"


void println(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    return ;
}

bool IsStrSame(char *A, char *B) {
    return (strncmp(A, B, strlen(B)) == 0);
}

//===================================================================
// Error
//===================================================================
void Error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[ERROR] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

void verror_at(char *file, char *input, char *loc, char *msg, va_list ap) {
    char *line = loc;
    while (input < line && line[-1] != '\n')
        line--;

    char *end = loc;
    while (*end != '\n' &&*end != '\0')
        end++;

    int line_num = 1;
    for (char *p = input; p < line; p++) {
        if (*p == '\n')
            line_num++;
    }

    int indent = fprintf(stderr, "%s:%d:", file, line_num);
    fprintf(stderr, "%.*s\n", (int)(end - line), line);

    int pos = loc - line + indent;
    fprintf(stderr, "%*s", pos, "");
    fprintf(stderr, "^ ");
    vfprintf(stderr, msg, ap);
    fprintf(stderr, "\n");
}

void ErrorAt(char *loc, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at(FileName, UserInput, loc, fmt, ap);
    exit(1);
}

void ErrorToken(Token *tok, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at(FileName, UserInput, tok->loc, fmt, ap);
    exit(1);
}

//===================================================================
// Debug
//===================================================================
void Debug(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[DEBUG] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    return;
}

void PrintToken(Token *tok) {
    for (Token *t = tok; t; t = t->next) {
        switch (t->kind) {
        case TK_NUM:
            Debug("Number");
            continue;
        case TK_RESERVED:
            Debug("Reserved");
            continue;
        case TK_IDENT:
            Debug("Ident");
            continue;
        case TK_STR:
            Debug("String");
            continue;
        case TK_EOF:
            Debug("End Of File");
            return;
        }
    }
}