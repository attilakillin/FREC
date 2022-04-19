CFLAGS = --std=c99 -ggdb -I../../libfrec/include -I../../libfrec/lib

all: grep-frec grep-posix grep-tre

clean:
	rm -f ./*.o
	rm -f grep-frec grep-posix grep-tre


grep-frec.o: grep-single.c
	gcc -c -o $@ $< $(CFLAGS) -DUSE_FREC

grep-posix.o: grep-single.c
	gcc -c -o $@ $< $(CFLAGS) -DUSE_POSIX

grep-tre.o: grep-single.c
	gcc -c -o $@ $< $(CFLAGS) -DUSE_TRE


grep-frec: grep-frec.o
	gcc -o $@ $< -L../../libfrec/lib -lfrec -ltre

grep-posix: grep-posix.o
	gcc -o $@ $<

grep-tre: grep-tre.o
	gcc -o $@ $< -ltre
