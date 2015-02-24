LLVM_DIR=/usr/lib/llvm-3.4/bin
CC=clang++-3.5
LLVM_CONFIG=$(LLVM_DIR)/llvm-config
CFLAGS=-I include -O3 `$(LLVM_CONFIG) --cxxflags`
LDFLAGS=-O3 `$(LLVM_CONFIG) --ldflags --libs core`

all: cli

obj/cli.o: src/cli.cpp include/ast.h include/parser.h
	$(CC) -c $<  -I include -o $@ $(CFLAGS)

obj/codegen.o: src/codegen.cpp include/ast.h
	$(CC) -c $<  -I include -o $@ $(CFLAGS)

obj/lexer.o: src/lexer.cpp include/lexer.h
	$(CC) -c $<  -I include -o $@ $(CFLAGS)

obj/parser.o: src/parser.cpp include/lexer.h include/ast.h include/parser.h
	$(CC) -c $<  -I include -o $@ $(CFLAGS)

cli: obj/cli.o obj/lexer.o obj/parser.o obj/codegen.o
	$(CC) $^ -o $@ $(LDFLAGS)

clean:
	rm -f obj/*.o
