#include <assert.h>

#include "5cc.h"

char *UserInput;

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
    //assert(depth == 0);

    return 0;
}