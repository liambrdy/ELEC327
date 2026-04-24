#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "lexer.h"
#include "darray.h"
#include "ast.h"
#include "arena.h"
#include "file.h"
#include "semantics.h"
#include "codegen/rom.h"

void usage(const char *exe) {
    printf("Usage: %s in_file [-o out_file] [-Iinc_dir] [-v]\n", exe);
}

typedef struct args_t {
    u8 *in_file; // required
    
    u8 *out_file; // not required
    u8 **inc_dirs; // not required

    bool verbose; // not required
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
                    if (*dir == '\0' && i + 1 < argc) dir = argv[++i]; // handle "-I path"
                    DArrayPush(outArgs->inc_dirs, dir);
                } break;

                case 'v': {
                    outArgs->verbose = true;
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

    ast_node_t *ast = AstFromTokens(tokens);
    if (!ast) {
        return 1;
    }

    AnnotateAst(ast);

    if (args.verbose) {
        PrintTokens(tokens);

        printf("Used %ld of %ld bytes after lexer\n", globalArena->pos, globalArena->capacity);
        
        PrintAst(ast, 0);
    
        printf("Used %ld of %ld bytes after ast\n", globalArena->pos, globalArena->capacity);
        printf("Used %d bytes by darray\n", bytesAllocated);
    }

    u32 romSize = 0;
    u8 *rom = CodegenRom(ast, &romSize);
    if (!args.out_file) {
        args.out_file = "a.out";
    }
    WriteFile(args.out_file, rom, romSize);
    free(rom);

    free(globalArenaMem);

    return 0;
}