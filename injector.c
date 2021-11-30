#include <elf.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include "injector.h"

#define PAGE_SIZE (1024*4)

void* open_file_to_mem(int fd, size_t* file_length) {
    struct stat f_state = {};
    fstat(fd, &f_state);
    void* data = mmap(NULL, f_state.st_size, PROT_WRITE, MAP_PRIVATE, fd, 0);
    *file_length = f_state.st_size;
    return data;
}

const char* get_shstrtab_start(void* elf_file_start) {
    Elf64_Ehdr* header = (Elf64_Ehdr*)elf_file_start;
    Elf64_Shdr* shstrtab_section = (Elf64_Shdr*)((char*)elf_file_start +header->e_shoff
            + header->e_shstrndx * header->e_shentsize);
    return (char *)elf_file_start + shstrtab_section->sh_offset;
}


int inject_code_to_elf(const char* file_path, void* data, size_t inject_len, const char* save_at) {
    size_t file_size;
    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "open source file error\n");
        return OPEN_ORIGIN_FILE_ERROR;
    }
    // jmp rel32
    unsigned char to_origin_code[] = {0xE9, 0x00, 0x00, 0x00, 0x00};
    void* file_mem = open_file_to_mem(fd, &file_size);
    void* save_file_mem = malloc(file_size + PAGE_SIZE);
    Elf64_Ehdr* new_header = (Elf64_Ehdr*) save_file_mem;
    memset(save_file_mem, 0, file_size + PAGE_SIZE);


    memcpy(save_file_mem, file_mem, sizeof(Elf64_Ehdr));
    const char* shstrtab_start = get_shstrtab_start(file_mem);


    // find text section
    Elf64_Shdr* text_section = NULL;
    {
        char* section_base = (char*)file_mem + ((Elf64_Ehdr*)file_mem)->e_shoff;
        for(int section_index = 0; section_index < ((Elf64_Ehdr*)file_mem)->e_shnum; section_index++) {
            Elf64_Shdr* section = (Elf64_Shdr*) (section_base +
                                                  section_index * ((Elf64_Ehdr *) file_mem)->e_shentsize);
            if (strcmp(".text", shstrtab_start + section->sh_name) == 0) {
                text_section = section;
                break;
            }
        }
    }

    // find segment that .text section at and copy program headers to new file
    Elf64_Phdr* text_segment = NULL;
    char* program_headers_base = (char*)file_mem + ((Elf64_Ehdr*)file_mem)->e_phoff;
    {
        for(int segment_index = 0; segment_index < ((Elf64_Ehdr*)file_mem)->e_phnum; segment_index++) {
            Elf64_Phdr* segment = (Elf64_Phdr *) (program_headers_base +
                                                  segment_index * ((Elf64_Ehdr *) file_mem)->e_phentsize);
            // copy program header to new file
            char* new_program_header_at = (char*)save_file_mem + ((Elf64_Ehdr*)file_mem)->e_phoff +
                                          segment_index * ((Elf64_Ehdr *) file_mem)->e_phentsize;
            memcpy(new_program_header_at, segment, ((Elf64_Ehdr *) file_mem)->e_phentsize);
            if (segment->p_offset < text_section->sh_offset &&
                    segment->p_offset + segment->p_filesz > text_section->sh_offset
            ) {
                text_segment = segment;
                ((Elf64_Phdr*)new_program_header_at)->p_filesz += inject_len + sizeof(to_origin_code);
                ((Elf64_Phdr*)new_program_header_at)->p_memsz += inject_len + sizeof(to_origin_code);
            }

        }
    }


    // set offset of segments after .text
    for (int segment_index = 0; segment_index < ((Elf64_Ehdr*)file_mem)->e_phnum; segment_index++) {
        Elf64_Phdr* segment = (Elf64_Phdr *) (program_headers_base +
                                              segment_index * ((Elf64_Ehdr *) file_mem)->e_phentsize);
        if (segment->p_offset > text_segment->p_offset) {
            Elf64_Phdr* save_segment = (Elf64_Phdr *) ((char *)save_file_mem + ((Elf64_Ehdr*)save_file_mem)->e_phoff +
                                                       segment_index * ((Elf64_Ehdr *) file_mem)->e_phentsize);
            save_segment->p_offset += PAGE_SIZE;
        }
    }


    Elf64_Shdr* last_section_in_text_segment = NULL;
    size_t section_header_off = PAGE_SIZE;
    if (((Elf64_Ehdr*)file_mem)->e_shoff < text_section->sh_offset) {
        section_header_off = 0;
    }
    {
        char* section_base = (char*)file_mem + ((Elf64_Ehdr*)file_mem)->e_shoff;
        char* new_section_base = (char*)save_file_mem + ((Elf64_Ehdr*)file_mem)->e_shoff + section_header_off;
        // find last section of text segment
        for(int section_index = 0; section_index < ((Elf64_Ehdr*)file_mem)->e_shnum; section_index++) {
            Elf64_Shdr* section = (Elf64_Shdr*) (section_base +
                                                 section_index * ((Elf64_Ehdr *) file_mem)->e_shentsize);
            if (section->sh_offset > text_segment->p_offset && section->sh_offset < text_segment->p_offset + text_segment->p_memsz) {
                if (last_section_in_text_segment == NULL || section->sh_offset > last_section_in_text_segment->sh_offset) {
                    last_section_in_text_segment = section;
                }
            }
        }

        // set to origin code
        {
            size_t entry = ((Elf64_Ehdr*)file_mem)->e_entry;
            size_t trampoline_start = last_section_in_text_segment->sh_addr
                                      + last_section_in_text_segment->sh_size + inject_len + 5; // 5 is the jmp ins len
            int32_t two_entry_offset = (int32_t)(entry - trampoline_start);
            memcpy(to_origin_code + 1, &two_entry_offset, sizeof(int32_t));
        }

        for(int section_index = 0; section_index < ((Elf64_Ehdr*)file_mem)->e_shnum; section_index++) {
            Elf64_Shdr* section = (Elf64_Shdr*) (section_base +
                                                 section_index * ((Elf64_Ehdr *) file_mem)->e_shentsize);
            Elf64_Shdr* new_section = (Elf64_Shdr*) (new_section_base +
                                                     section_index * ((Elf64_Ehdr *) file_mem)->e_shentsize);
            *new_section = *section;
            if (section == last_section_in_text_segment) {
                // inject code
                size_t max_inject_len = PAGE_SIZE - (text_segment->p_offset + text_segment->p_filesz) % PAGE_SIZE;
                if (max_inject_len < inject_len) {
                    fprintf(stderr,"max inject len: %zu, but inject len is: %zu\n", max_inject_len, inject_len);
                    return NO_ENOUGH_SPACE;
                }
                new_section->sh_size += inject_len + sizeof(to_origin_code);
                memcpy((char*)save_file_mem + section->sh_offset + section->sh_size, data, inject_len);
                memcpy((char*)save_file_mem + section->sh_offset + section->sh_size + inject_len, to_origin_code,
                       sizeof(to_origin_code));
            } else if (section->sh_offset > last_section_in_text_segment->sh_offset) {
                new_section->sh_offset += PAGE_SIZE;
            }
            memcpy((char*)save_file_mem + new_section->sh_offset, (char*)file_mem + section->sh_offset, section->sh_size);
        }
    }


    new_header->e_shoff += section_header_off; // reset e_shoff
    new_header->e_entry = last_section_in_text_segment->sh_addr+last_section_in_text_segment->sh_size;
    munmap(file_mem, file_size);
    close(fd);
    int save_fd = open(save_at, O_RDWR | O_CREAT, S_IRUSR | S_IRGRP | S_IROTH | S_IXUSR | S_IWUSR );
    if (save_fd != -1) {
        write(save_fd, save_file_mem, file_size + PAGE_SIZE);
        free(save_file_mem);
    } else {
        fprintf(stderr, "create output file error\n");
        free(save_file_mem);
        return CREATE_OUTPUT_FILE_ERROR;
    }
    close(save_fd);

    return 0;

}

