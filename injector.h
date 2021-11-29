#pragma once
#include <stdlib.h>


int inject_code_to_elf(const char* file_path, void* data, size_t inject_len, const char* save_at);