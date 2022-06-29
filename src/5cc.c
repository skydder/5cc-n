#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

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

int main(int argc, char **argv) {
    if (argc != 2) {
        Error("引数の個数が正しくありません");
        return 1;
    }
    char *p = argv[1];

    println(".intel_syntax noprefix");
    println(".globl main");
    println("main:");
    println("\tmov rax, %ld", strtol(p, &p, 10));

    while (*p) {
        if (*p == '+') {
            p++;
            println("\tadd rax, %ld", strtol(p, &p, 10));
            continue;
        }

        if (*p == '-') {
            p++;
            println("\tsub rax, %ld", strtol(p, &p, 10));
            continue; 
        }

        Error("Unexpected letter: '%c'", *p);
    }
    println("\tret");
    
    return 0;
}