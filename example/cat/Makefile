CLI=../../cli

all: cat

cat.ll: cat.eric $(CLI)
	cat $< | $(CLI) -c $< 2> $@

cat.s: cat.ll
	llc -o $@ $<

cat: cat.s
	clang++-3.5 -o $@ $<

clean:
	rm -f *.ll *.s cat
