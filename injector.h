#pragma once
#include <stdlib.h>

enum {
    NO_ENOUGH_SPACE = 5,
    OPEN_ORIGIN_FILE_ERROR,
    CREATE_OUTPUT_FILE_ERROR,
};

int inject_code_to_elf(const char* file_path, void* data, size_t inject_len, const char* save_at);