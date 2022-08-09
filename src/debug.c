#include "5cc.h"

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