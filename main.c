#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "injector.h"

struct cmd_arguments {
    char* source_file;
    int overwrite;
    char* save_file;
    char* code_file;
};

void print_help() {
    printf("-o\toutput file\n");
    printf("-i\tinput file\n");
    printf("-c\tcode file\n");
}

int parse_arguments(struct cmd_arguments* arguments, int argc, char **argv) {
    int output_write = 0, input_write = 0, code_write = 0;
    for(int arg_index = 1; arg_index < argc; arg_index++) {
        if(strcmp(argv[arg_index], "-o") == 0) {
            if (output_write != 0) {
                fprintf(stderr, "two much -o argument\n");
                return -1;
            }
            if (arg_index + 1 < argc ) {
                arguments->save_file = argv[arg_index+1];
                arg_index++; // pass path
                output_write = 1;
            } else {
                return -1;
            }
        } else if (strcmp(argv[arg_index], "-i") == 0) {
            if (input_write != 0) {
                fprintf(stderr, "two much input files\n");
                return -1;
            }
            if (arg_index + 1 < argc) {
                arguments->source_file = argv[arg_index+1];
                arg_index++; // pass path
                input_write = 1;
            } else {
                return -1;
            }
        } else if(strcmp(argv[arg_index], "-c") == 0 || strcmp(argv[arg_index], "--code") == 0) {
            if (code_write != 0) {
                fprintf(stderr, "two much code\n");
                return -1;
            } else {
                arguments->code_file = argv[arg_index+1];
                arg_index++; // pass path
                code_write = 1;
            }
        } else {
            if (input_write == 0) {
                arguments->save_file = argv[arg_index];
                input_write = 1;
            } else {
                fprintf(stderr, "two much input files\n");
                return -1;
            }
        }
    }

    if (arguments->source_file == NULL) {
        fprintf(stderr, "no source file\n");
        return -1;
    }

    if (arguments->code_file == NULL) {
        fprintf(stderr, "no code file\n");
        return -1;
    }

    if (arguments->save_file == NULL) {
        arguments->overwrite = 1;
        arguments->save_file = arguments->source_file;
    }

    return 0;
}

const char* parse_inject_code(const char* source, size_t* len) {
    int fd = open(source, O_RDONLY);
    if (fd == -1) {
        return NULL;
    }
    struct stat f_state;
    fstat(fd, &f_state);
    char* data = malloc(f_state.st_size);
    read(fd, data, f_state.st_size);
    *len = f_state.st_size;
    return data;
}

int main(int argc, char **argv) {
    struct cmd_arguments arguments;
    if (argc == 1) {
        printf("no enough argument\n\n");
        print_help();
        return 0;
    }
    if(parse_arguments(&arguments, argc, argv) == 0) {
        size_t code_len;
        const char *data = parse_inject_code(arguments.code_file, &code_len);
        inject_code_to_elf(arguments.source_file, (void *)data, code_len, arguments.save_file);
    }

    printf("Write new elf executable file success.\n");

    return 0;
}
