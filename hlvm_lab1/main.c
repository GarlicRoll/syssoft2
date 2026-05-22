#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cfg.h"
#include "clr_model.h"
#include "clr_serializer.h"
#include "parser.h"

static void sanitize_name(const char *src, char *dst, size_t dst_size) {
    if (!dst || dst_size == 0) return;
    dst[0] = '\0';
    if (!src || !*src) {
        snprintf(dst, dst_size, "HlvmLab1Clr");
        return;
    }

    size_t di = 0;
    if (!((src[0] >= 'A' && src[0] <= 'Z') || (src[0] >= 'a' && src[0] <= 'z') || src[0] == '_')) {
        if (di + 1 < dst_size) dst[di++] = '_';
    }

    for (size_t i = 0; src[i] && di + 1 < dst_size; i++) {
        char c = src[i];
        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '_') {
            dst[di++] = c;
        } else {
            dst[di++] = '_';
        }
    }
    dst[di] = '\0';

    if (dst[0] == '\0') {
        snprintf(dst, dst_size, "HlvmLab1Clr");
    }
}

static void derive_names(const char *output_path, char *assembly, size_t assembly_size, char *class_name,
                         size_t class_size) {
    char path_buf[1024];
    strncpy(path_buf, output_path, sizeof(path_buf) - 1);
    path_buf[sizeof(path_buf) - 1] = '\0';

    char *base = basename(path_buf);
    char stem[512];
    strncpy(stem, base ? base : "program", sizeof(stem) - 1);
    stem[sizeof(stem) - 1] = '\0';

    char *dot = strrchr(stem, '.');
    if (dot) *dot = '\0';

    sanitize_name(stem, assembly, assembly_size);
    sanitize_name(stem, class_name, class_size);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <output.il> <input1> [input2 ...]\n", argv[0]);
        return 1;
    }

    const char *output_path = argv[1];
    const int input_count = argc - 2;

    FilenameParseTree *inputs = (FilenameParseTree *)calloc((size_t)input_count, sizeof(FilenameParseTree));
    ParseResult **parse_results = (ParseResult **)calloc((size_t)input_count, sizeof(ParseResult *));
    if (!inputs || !parse_results) {
        fprintf(stderr, "Out of memory\n");
        free(inputs);
        free(parse_results);
        return 1;
    }

    int had_errors = 0;
    for (int i = 0; i < input_count; i++) {
        const char *file_path = argv[i + 2];
        FILE *in = fopen(file_path, "r");
        if (!in) {
            fprintf(stderr, "Cannot open input file: %s\n", file_path);
            had_errors = 1;
            break;
        }

        ParseResult *parsed = parse(in);
        fclose(in);

        parse_results[i] = parsed;
        inputs[i].filename = argv[i + 2];
        inputs[i].tree = parsed;

        for (int e = 0; e < parsed->errorsCount; e++) {
            fprintf(stderr, "%s", parsed->errors[e]);
        }
    }

    if (!had_errors) {
        Array *graph = executionGraph(inputs, input_count);

        char assembly_name[256];
        char class_name[256];
        derive_names(output_path, assembly_name, sizeof(assembly_name), class_name, sizeof(class_name));

        ClrProgramModel model;
        char *build_error = NULL;
        if (clr_build_program_model(graph, assembly_name, class_name, &model, &build_error) != 0) {
            fprintf(stderr, "CLR model build failed: %s\n", build_error ? build_error : "unknown error");
            free(build_error);
            had_errors = 1;
        } else {
            if (clr_serialize_il(&model, output_path) != 0) {
                fprintf(stderr, "Failed to write output file: %s\n", output_path);
                had_errors = 1;
            }
            clr_free_program_model(&model);
        }
    }

    for (int i = 0; i < input_count; i++) {
        if (parse_results[i]) {
            freeMem(parse_results[i]);
        }
    }

    free(parse_results);
    free(inputs);

    return had_errors ? 1 : 0;
}
