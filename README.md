# elf64-text-inject 
## Introduction 
This is a tool can inject code to an executable elf file. After injecting, the entrypoint of the elf will be replace to the inject code, and injector will append an instruction which help the program jump to the origin entry at the end of the code you inject.
The injector only support amd64 now because only amd64 recover trampoline has been implemented.
## Principle 
The code injected will be inject to the end of segment that `.text` section at because this segment is executable and there will be some empty virtual memory space due to memory align. 
As shown below, the fourth segment is executable and the segment align is `0x1000` and it end at `0x1000 + 0x2f5 = 0x12f5` with the fifth segment start at `0x2000`, so the virtual memory address between `0x12f5` and `0x2000` will not be used after program loaded which means we can inject code to the space with other segments' virtual address not be changed.  

```
Program Headers:
  Type           Offset             VirtAddr           PhysAddr
                 FileSiz            MemSiz              Flags  Align
  PHDR           0x0000000000000040 0x0000000000000040 0x0000000000000040
                 0x00000000000002d8 0x00000000000002d8  R      0x8
  INTERP         0x0000000000000318 0x0000000000000318 0x0000000000000318
                 0x000000000000001c 0x000000000000001c  R      0x1
      [Requesting program interpreter: /lib64/ld-linux-x86-64.so.2]
  LOAD           0x0000000000000000 0x0000000000000000 0x0000000000000000
                 0x00000000000006f8 0x00000000000006f8  R      0x1000
  LOAD           0x0000000000001000 0x0000000000001000 0x0000000000001000
                 0x00000000000002f5 0x00000000000002f5  R E    0x1000
  LOAD           0x0000000000002000 0x0000000000002000 0x0000000000002000
                 0x00000000000001b8 0x00000000000001b8  R      0x1000
  LOAD           0x0000000000002d90 0x0000000000003d90 0x0000000000003d90
                 0x0000000000000284 0x0000000000000288  RW     0x1000
  DYNAMIC        0x0000000000002da0 0x0000000000003da0 0x0000000000003da0
                 0x0000000000000200 0x0000000000000200  RW     0x8
  NOTE           0x0000000000000338 0x0000000000000338 0x0000000000000338
                 0x0000000000000020 0x0000000000000020  R      0x8
  NOTE           0x0000000000000358 0x0000000000000358 0x0000000000000358
                 0x0000000000000044 0x0000000000000044  R      0x4
  GNU_PROPERTY   0x0000000000000338 0x0000000000000338 0x0000000000000338
                 0x0000000000000020 0x0000000000000020  R      0x8
  GNU_EH_FRAME   0x0000000000002044 0x0000000000002044 0x0000000000002044
                 0x000000000000004c 0x000000000000004c  R      0x4
  GNU_STACK      0x0000000000000000 0x0000000000000000 0x0000000000000000
                 0x0000000000000000 0x0000000000000000  RW     0x10
  GNU_RELRO      0x0000000000002d90 0x0000000000003d90 0x0000000000003d90
                 0x0000000000000270 0x0000000000000270  R      0x1
``` 
## Shortcoming 
In some situations, there may be not enough space to inject. In the worst case, there are no space to inject if the last section of the segment will be filled exactly aligned.