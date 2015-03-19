CLI=../../cli

all: prime

prime.ll: prime.eric $(CLI)
	cat $< | $(CLI) -c $< 2> $@

prime.s: prime.ll
	llc -O=0 -o $@ $<

prime: prime.s
	clang++-3.5 -O0 -o $@ $<

clean:
	rm -f *.ll *.s prime
