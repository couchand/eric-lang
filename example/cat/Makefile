CLI=../../cli

all: cat

cat.ll: cat.eric $(CLI)
	cat $< | $(CLI) -c $< 2> $@

cat.s: cat.ll
	llc -O=0 -o $@ $<

cat: cat.s
	clang++-3.5 -O0 -o $@ $<

clean:
	rm -f *.ll *.s cat
