CLI=../../cli

all: circle

circle.ll: circle.eric $(CLI)
	cat $< | $(CLI) -c $< 2> $@

circle.s: circle.ll
	llc -O=0 -o $@ $<

circle: circle.s
	clang++-3.5 -O0 -o $@ $<

clean:
	rm -f *.ll *.s circle
