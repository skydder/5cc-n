#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "5cc.h"


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
    verror_at(InputPath, UserInput, loc, fmt, ap);
    exit(1);
}

void ErrorToken(Token *tok, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at(InputPath, UserInput, tok->loc, fmt, ap);
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
            Debug("-: %s", t->string);
            continue;
        case TK_EOF:
            Debug("End Of File");
            return;
        }
    }
}

void PrintNode(Node *node) {
    #define DEBUG_NODE(token) case token:Debug(#token);continue;
    for (Node *n = node; n; n = n->next) {
        switch (n->kind) {
        DEBUG_NODE(ND_ADD);
        DEBUG_NODE(ND_SUB);
        DEBUG_NODE(ND_MUL);
        DEBUG_NODE(ND_DIV);
        DEBUG_NODE(ND_NEG);
        DEBUG_NODE(ND_NUM);
        DEBUG_NODE(ND_EQ); // ==
        DEBUG_NODE(ND_NE); // !=
        DEBUG_NODE(ND_LT); // <
        DEBUG_NODE(ND_LE); // <=
        DEBUG_NODE(ND_ASSIGN);
        DEBUG_NODE(ND_VAR);
        
        DEBUG_NODE(ND_RETURN);
        
        DEBUG_NODE(ND_IF);
        DEBUG_NODE(ND_FOR);
        DEBUG_NODE(ND_ADDR);
        DEBUG_NODE(ND_DEREF);
        DEBUG_NODE(ND_FNCALL);
        DEBUG_NODE(ND_COMMA);
        DEBUG_NODE(ND_DOTS);  // struct or union
        DEBUG_NODE(ND_STMT_EXPR);
        DEBUG_NODE(ND_EXPR_STMT);
        case ND_BLOCK:
            Debug("ND_BLOCK");
            PrintNode(n->body);
            continue;
        }
    }
    #undef DEBUG_NODE
}

void PrintObjFn(Obj *obj) {
     for (Obj *fn = obj; fn; fn = fn->next) {
        if (!fn->is_func || !fn->is_def) continue;
        Debug("FUNCTION");
        PrintNode(fn->body);
     }
}