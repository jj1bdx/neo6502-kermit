/Users/kenji/bin/llvm-mos/bin/mos-neo6502-clang -Os -I/Users/kenji/bin/llvm-mos/mos-platform/neo6502/include -I/Users/kenji/bin/llvm-mos/mos-platform/common/include -DDEBUG   -emit-llvm -S -o main.ll main.c
/Users/kenji/bin/llvm-mos/bin/mos-neo6502-clang -Os -I/Users/kenji/bin/llvm-mos/mos-platform/neo6502/include -I/Users/kenji/bin/llvm-mos/mos-platform/common/include -DDEBUG   -emit-llvm -S -o kermit.ll kermit.c
/Users/kenji/bin/llvm-mos/bin/mos-neo6502-clang -Os -I/Users/kenji/bin/llvm-mos/mos-platform/neo6502/include -I/Users/kenji/bin/llvm-mos/mos-platform/common/include -DDEBUG   -emit-llvm -S -o neoio.ll neoio.c
/Users/kenji/bin/llvm-mos/bin/mos-neo6502-clang -Os -I/Users/kenji/bin/llvm-mos/mos-platform/neo6502/include -I/Users/kenji/bin/llvm-mos/mos-platform/common/include -DDEBUG   -c -o main.o main.c
/Users/kenji/bin/llvm-mos/bin/mos-neo6502-clang -Os -I/Users/kenji/bin/llvm-mos/mos-platform/neo6502/include -I/Users/kenji/bin/llvm-mos/mos-platform/common/include -DDEBUG   -c -o kermit.o kermit.c
/Users/kenji/bin/llvm-mos/bin/mos-neo6502-clang -Os -I/Users/kenji/bin/llvm-mos/mos-platform/neo6502/include -I/Users/kenji/bin/llvm-mos/mos-platform/common/include -DDEBUG   -c -o neoio.o neoio.c
/Users/kenji/bin/llvm-mos/bin/mos-neo6502-clang -Os -I/Users/kenji/bin/llvm-mos/mos-platform/neo6502/include -I/Users/kenji/bin/llvm-mos/mos-platform/common/include -DDEBUG -o kermit.neo.s -flto -Wl,--lto-emit-asm main.o kermit.o neoio.o
/Users/kenji/bin/llvm-mos/bin/llvm-objdump -Sst kermit.neo.elf > kermit.neo.elf.txt
