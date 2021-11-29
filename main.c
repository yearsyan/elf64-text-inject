#include <stdio.h>
#include "injector.h"

int main(int argc, char **argv) {
    char *data = "\x90"; //"\x48\x8D\x05\xC4\xFD\xFF\xFF\xFF\xE0";
    inject_code_to_elf("/home/yan/reserve/gApp", data, 1, "/home/yan/reserve/gApp2");
    return 0;
}
