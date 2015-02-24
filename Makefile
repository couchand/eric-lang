all: cli

obj/lexer.o: src/lexer.cpp include/lexer.h
	clang++-3.5 -c $<  -I include -o $@

obj/parser.o: src/parser.cpp include/lexer.h include/ast.h include/parser.h
	clang++-3.5 -c $<  -I include -o $@

obj/cli.o: src/cli.cpp include/ast.h include/parser.h
	clang++-3.5 -c $<  -I include -o $@

cli: obj/cli.o obj/lexer.o obj/parser.o
	clang++-3.5 $^ -o $@

clean:
	rm -f obj/*.o
