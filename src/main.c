#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "5cc.h"

static char *opt_o;
static char *opt_c;
static bool opt_D;

char *InputPath;
char *UserInput;

static void usage(int status) {
    fprintf(stderr, "5cc [ -o <path> || -c <cmd>] <file>\n");
    exit(status);
}

static char *ReadFile(char *path) {
    FILE *fp;
    if (!strcmp(path, "-")) {
        fp = stdin;
    } else {
        fp = fopen(path, "r");
        if (!fp) Error("can't open file %s: %s", path, strerror(errno));
    }
    

    char *buf;
    size_t buflen;
    FILE *out = open_memstream(&buf, &buflen);

    for (;;) {
        char buf2[4096];
        int n = fread(buf2, 1, sizeof(buf2), fp);
        if (n == 0)
            break;
        fwrite(buf2, 1, n, out);
    }

    if (fp != stdin)
        fclose(fp);

    fflush(out);
    if (buflen == 0 || buf[buflen - 1] != '\n')
        fputc('\n', out);
    fputc('\0', out);
    fclose(out);
    return buf;
}

void Compile(char *code, FILE *out) {
    Token *token = Tokenize(code);
    if (opt_D) PrintToken(token);
    Obj *node = ParseToken(token);
    if (opt_D) PrintObjFn(node);
    GenCode(node, out);
}

static void ParseArgs(int argc, char **argv) {
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--help")) {
            usage(0);
        }
        if (!strcmp(argv[i], "-o")) {
            if (!argv[++i]) usage(1);
            opt_o = argv[i];
            continue;
        } else if (IsStrSame(argv[i], "-o")) {
            opt_o = argv[i] + 2;
            continue;
        }
        if (!strcmp(argv[i], "-c")) {
            if (!argv[++i]) usage(1);
            opt_c = argv[i];
            InputPath = "<arg>:";
            continue;
        }
        if (!strcmp(argv[i], "-D")) {
            opt_D = true;
            continue;
        }
        if (argv[i][0] == '-' && argv[i][1] != '\0')
            Error("unknown argument: %s", argv[i]);

        InputPath = argv[i];
    }
    if (!IsStrSame(InputPath, "<arg>:") && opt_c) Error("invaild argument");
    if (!InputPath) Error("no input files");
    
}

static FILE *OpenFile(char *path) {
    if (!path || strcmp(path, "-") == 0)
        return stdout;

    FILE *out = fopen(path, "w");
    if (!out)
        Error("cannot open output file: %s: %s", path, strerror(errno));
    return out;
}

static char *ReadCode() {
    if (opt_c)
        return opt_c;
    return ReadFile(InputPath);
}

int main(int argc, char **argv) {
    if (argc < 2)
        usage(1);
    ParseArgs(argc, argv);
    UserInput = ReadCode();
    Compile(UserInput, OpenFile(opt_o));

    return 0;
}