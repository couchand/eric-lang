CLI=../../cli

all: riddle

riddle.ll: riddle.eric $(CLI)
	cat $< | $(CLI) -c $< 2> $@

riddle.s: riddle.ll
	llc -O=0 -o $@ $<

riddle: riddle.s
	clang++-3.5 -O0 -o $@ $<

clean:
	rm -f *.ll *.s riddle
