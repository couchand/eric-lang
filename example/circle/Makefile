CLI=../../cli

all: circle

circle.ll: circle.eric $(CLI)
	cat $< | $(CLI) -c $< 2> $@

circle.s: circle.ll
	llc -o $@ $<

circle: circle.s
	clang++-3.5 -o $@ $<

clean:
	rm -f *.ll *.s circle
