#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

void Error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
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
    
    println(".intel_syntax noprefix");
    println(".globl main");
    println("main:");
    println("\tmov rax, %d", atoi(argv[1]));
    println("\tret");
    
    return 0;
}