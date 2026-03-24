#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "lexer.h"
#include "darray.h"
#include "ast.h"
#include "arena.h"
#include "file.h"
#include "semantics.h"

void usage(const char *exe) {
    printf("Usage: %s in_file [-o out_file] [-Iinc_dir]\n", exe);
}

typedef struct args_t {
    u8 *in_file; // required
    
    u8 *out_file; // not required
    u8 **inc_dirs; // not required
} args_t;

int parse(int argc, char **argv, args_t *outArgs) {
    if (argc < 1) {
        usage(argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        char *next = argv[i];
        if (next[0] == '-') {
            switch (next[1]) {
                case 'o': {
                    i++;
                    outArgs->out_file = argv[i];
                } break;

                case 'I': {
                    if (!outArgs->inc_dirs) {
                        outArgs->inc_dirs = DArrayCreate(u8 *);
                    }
                    u8 *dir = argv[i] + 2;
                    DArrayPush(outArgs->inc_dirs, dir);
                } break;

                default: {
                    usage(argv[0]);
                    printf("Unknown flag: %s\n", next);
                    return 1;
                } break;
            }
        } else {
            outArgs->in_file = next;
        }
    }

    if (outArgs->in_file == NULL) {
        usage(argv[0]);
        printf("No input files\n");
        return 1;
    }

    return 0;
}

int main(int argc, char **argv) {
    args_t args = {0};
    int result = parse(argc, argv, &args);
    if (result != 0) {
        return result;
    }

    u64 globalArenaSize = Megabytes(100);
    u8 *globalArenaMem = (u8 *)malloc(globalArenaSize);
    if (!globalArenaMem) {
        printf("Failed to allocate memory");
        return 1;
    }

    globalArena = CreateArena(globalArenaMem, globalArenaSize);

    loaded_file_t file = LoadFile(args.in_file);
    if (!file.success) {
        return 1;
    }

    token_t *tokens = tokenize(file.buffer, file.bufferLen, args.in_file, args.inc_dirs);
    if (!tokens) {
        return 1;
    }

    PrintTokens(tokens);

    printf("Used %ld of %ld bytes after lexer\n", globalArena->pos, globalArena->capacity);

    ast_node_t *ast = AstFromTokens(tokens);
    if (!ast) {
        return 1;
    }

    PrintAst(ast, 0);

    AnnotateAst(ast);

    printf("Used %ld of %ld bytes after ast\n", globalArena->pos, globalArena->capacity);
    printf("Used %d bytes by darray\n", bytesAllocated);

    free(globalArenaMem);

    return 0;
}