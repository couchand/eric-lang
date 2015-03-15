CLI=../../cli

all: sieve

sieve.ll: sieve.eric $(CLI)
	cat $< | $(CLI) -c $< 2> $@

sieve.s: sieve.ll
	llc -o $@ $<

sieve: sieve.s
	clang++-3.5 -o $@ $<

clean:
	rm -f *.ll *.s sieve
