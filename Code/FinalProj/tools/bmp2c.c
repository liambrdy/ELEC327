#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_ONLY_JPEG
#include "stb_image.h"

/* COLOR_MAGENTA in RGB565 — used as chroma key for transparency */
#define CHROMA_KEY 63519

static uint16_t rgb_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static void make_ident(const char *in, char *out, int maxlen) {
    int j = 0;
    for (int i = 0; in[i] && j < maxlen - 1; i++) {
        char c = in[i];
        if (isalnum((unsigned char)c)) {
            out[j++] = (char)tolower((unsigned char)c);
        } else if (j > 0 && out[j - 1] != '_') {
            out[j++] = '_';
        }
    }
    while (j > 0 && out[j - 1] == '_') j--;
    out[j] = '\0';
}

static void usage(const char *exe) {
    fprintf(stderr,
        "Usage: %s <image> [-n name] [-o out.h] [-t R G B]\n"
        "\n"
        "  <image>       PNG, BMP, or JPEG input file\n"
        "  -n name       variable name prefix (default: derived from filename)\n"
        "  -o out.h      output file (default: stdout)\n"
        "  -t R G B      mark pixels of this RGB color (0-255 each) as transparent\n"
        "                (replaces them with COLOR_MAGENTA = %d, the chroma key)\n"
        "\n"
        "Output: a .h file with:\n"
        "  #define NAME_W <width>\n"
        "  #define NAME_H <height>\n"
        "  int name_pixels[W*H] = { ... };  /* RGB565, one int per pixel */\n",
        exe, CHROMA_KEY);
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(argv[0]); return 1; }

    const char *infile  = argv[1];
    const char *outfile = NULL;
    char name[64]       = {0};
    int has_trans       = 0;
    uint8_t tr = 0, tg = 0, tb = 0;

    /* derive default name from filename */
    const char *base = strrchr(infile, '/');
    base = base ? base + 1 : infile;
    char basename_buf[256];
    strncpy(basename_buf, base, sizeof(basename_buf) - 1);
    basename_buf[sizeof(basename_buf) - 1] = '\0';
    char *dot = strrchr(basename_buf, '.');
    if (dot) *dot = '\0';
    make_ident(basename_buf, name, sizeof(name));

    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "-n") && i + 1 < argc) {
            strncpy(name, argv[++i], sizeof(name) - 1);
        } else if (!strcmp(argv[i], "-o") && i + 1 < argc) {
            outfile = argv[++i];
        } else if (!strcmp(argv[i], "-t") && i + 3 < argc) {
            has_trans = 1;
            tr = (uint8_t)atoi(argv[++i]);
            tg = (uint8_t)atoi(argv[++i]);
            tb = (uint8_t)atoi(argv[++i]);
        } else {
            fprintf(stderr, "bmp2c: unknown argument '%s'\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    if (name[0] == '\0') { strcpy(name, "bitmap"); }

    int w, h, channels;
    uint8_t *data = stbi_load(infile, &w, &h, &channels, 3);
    if (!data) {
        fprintf(stderr, "bmp2c: failed to load '%s': %s\n", infile, stbi_failure_reason());
        return 1;
    }

    FILE *out = outfile ? fopen(outfile, "w") : stdout;
    if (!out) {
        fprintf(stderr, "bmp2c: cannot open output '%s'\n", outfile);
        stbi_image_free(data);
        return 1;
    }

    /* upper-case name for header guard and defines */
    char upper[64];
    for (int i = 0; name[i]; i++) upper[i] = (char)toupper((unsigned char)name[i]);
    upper[strlen(name)] = '\0';

    fprintf(out, "#ifndef _%s_H\n", upper);
    fprintf(out, "#define _%s_H\n\n", upper);
    fprintf(out, "#define %s_W %d\n", upper, w);
    fprintf(out, "#define %s_H %d\n\n", upper, h);
    fprintf(out, "int %s_pixels[%d] = {\n", name, w * h);

    for (int y = 0; y < h; y++) {
        fprintf(out, "    ");
        for (int x = 0; x < w; x++) {
            int idx = (y * w + x) * 3;
            uint8_t r = data[idx + 0];
            uint8_t g = data[idx + 1];
            uint8_t b = data[idx + 2];

            uint16_t pixel;
            if (has_trans && r == tr && g == tg && b == tb) {
                pixel = CHROMA_KEY;
            } else {
                pixel = rgb_to_rgb565(r, g, b);
                /* avoid accidental chroma key collision */
                if (pixel == CHROMA_KEY && !has_trans) pixel = 63518;
            }

            if (x < w - 1 || y < h - 1)
                fprintf(out, "%u,", (unsigned)pixel);
            else
                fprintf(out, "%u", (unsigned)pixel);
        }
        fprintf(out, "\n");
    }

    fprintf(out, "};\n\n#endif\n");

    if (outfile) fclose(out);
    stbi_image_free(data);

    fprintf(stderr, "bmp2c: %s (%dx%d) -> %d pixels written to %s\n",
            infile, w, h, w * h, outfile ? outfile : "stdout");
    return 0;
}
