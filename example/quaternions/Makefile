CLI=../../cli

all: quaternions

quaternions.ll: quaternions.eric $(CLI)
	cat $< | $(CLI) -c $< 2> $@

quaternions.s: quaternions.ll
	llc -O=0 -o $@ $<

quaternions: quaternions.s
	clang++-3.5 -O0 -o $@ $<

clean:
	rm -f *.ll *.s quaternions
