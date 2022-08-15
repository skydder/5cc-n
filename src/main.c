#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "5cc.h"

char *FileName;
char *UserInput;

char *ReadFile(char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) Error("can't open file %s: %s", path, strerror(errno));

    if (fseek(fp, 0, SEEK_END) == -1)
        Error("%s: fseek: %s", path, strerror(errno));
    size_t size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) == -1)
        Error("%s: fseek: %s", path, strerror(errno));

    char *buf = calloc(sizeof(char), size + 2);
    fread(buf, size, 1, fp);

    if (size == 0 || buf[size - 1] != '\n')
        buf[size++] = '\n';
    buf[size] = '\0';
    fclose(fp);
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        Error("引数の個数が正しくありません");
        return 1;
    }
    
    FileName = argv[1];
    if (IsStrSame(argv[1], "-c")) {
        FileName = "<string>";
        UserInput = argv[2];
    } else
        UserInput = ReadFile(argv[1]);
    
    Token *token = Tokenize(UserInput);
    Obj *node = ParseToken(token);

    GenCode(node);

    return 0;
}