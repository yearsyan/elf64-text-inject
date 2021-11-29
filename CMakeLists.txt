cmake_minimum_required(VERSION 3.16)
project(elf64_text_inject C)

set(CMAKE_C_STANDARD 11)

add_executable(elf64_text_inject main.c injector.c injector.h)
add_compile_definitions(DEBUG)
