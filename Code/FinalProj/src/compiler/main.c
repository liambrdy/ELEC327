#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

void usage(const char *exe) {
    printf("Usage: %s in_file [-o out_file] \n", exe);
}

typedef struct args_t {
    char *in_file; // required
    
    char *out_file; // not required
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

    FILE *f = fopen(args.in_file, "r");
    if (!f) {
        printf("%s: no such file or directory\n", args.in_file);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long fileLength = ftell(f);
    rewind(f);

    uint8_t *buffer = (uint8_t *)malloc(fileLength + 1);
    fread(buffer, 1, fileLength, f);
    buffer[fileLength] = '\0';

    fclose(f);

    free(buffer);

    return 0;
}