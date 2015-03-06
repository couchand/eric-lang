CLI=../../cli

all: squares

squares.ll: squares.eric $(CLI)
	cat $< | $(CLI) -c $< 2> $@

squares.s: squares.ll
	llc -o $@ $<

squares: squares.s
	clang++-3.5 -o $@ $<

clean:
	rm -f *.ll *.s squares
