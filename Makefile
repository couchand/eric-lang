LLVM_DIR=/media/local/llvm/bin
CC=clang++-3.5
LLVM_CONFIG=$(LLVM_DIR)/llvm-config
CFLAGS=-I include -I /media/local/llvm-build/include -O3 `$(LLVM_CONFIG) --cxxflags`
LDFLAGS=-O3 `$(LLVM_CONFIG) --ldflags --libs core --system-libs`

all: cli

obj/builtins.o: src/builtins.cpp include/builtins.h include/ast.h
	$(CC) -c $< -o $@ $(CFLAGS)

obj/cli.o: src/cli.cpp include/ast.h include/parser.h include/codegen.h include/typecheck.h include/types.h include/builtins.h
	$(CC) -c $< -o $@ $(CFLAGS)

obj/codegen.o: src/codegen.cpp include/codegen.h include/ast.h include/types.h include/context.h
	$(CC) -c $< -o $@ $(CFLAGS)

obj/lexer.o: src/lexer.cpp include/lexer.h
	$(CC) -c $< -o $@ $(CFLAGS)

obj/parser.o: src/parser.cpp include/lexer.h include/ast.h include/parser.h
	$(CC) -c $< -o $@ $(CFLAGS)

obj/typecheck.o: src/typecheck.cpp include/typecheck.h include/ast.h include/types.h
	$(CC) -c $< -o $@ $(CFLAGS)

obj/types.o: src/types.cpp include/types.h include/context.h
	$(CC) -c $< -o $@ $(CFLAGS)

cli: obj/cli.o obj/lexer.o obj/parser.o obj/types.o obj/codegen.o obj/typecheck.o obj/builtins.o
	$(CC) $^ -o $@ $(LDFLAGS)

clean:
	rm -f obj/*.o
