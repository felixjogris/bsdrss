.PHONY:	clean test

bsdrss:	bsdrss.c Makefile
	cc -W -Wall -O3 -s -pipe -o bsdrss bsdrss.c

test:	bsdrss
	./bsdrss $$$$

clean: ;	-rm bsdrss
