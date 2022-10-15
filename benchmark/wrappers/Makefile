CFLAGS = --std=c99 -ggdb -I../../libfrec/include -I../../libfrec/lib

all: grep-frec grep-posix grep-tre grep-frec-multi grep-ff-frec grep-ff-tre grep-ff-posix

clean:
	rm -f ./*.o
	rm -f grep-frec grep-posix grep-tre grep-frec-multi
	rm -f grep-ff-frec grep-ff-posix grep-ff-tre

# Single pattern-matcher object and executable files

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

# Multi pattern-matcher object and executable files

grep-frec-multi.o: grep-multi.c
	gcc -c -o $@ $< $(CFLAGS) -DUSE_FREC

grep-frec-multi: grep-frec-multi.o
	gcc -o $@ $< -L../../libfrec/lib -lfrec -ltre

# Multi pattern-matcher object and executable files for first matching

grep-ff-frec.o: grep-ff-single.c
	gcc -c -o $@ $< $(CFLAGS) -DUSE_FREC

grep-ff-posix.o: grep-ff-single.c
	gcc -c -o $@ $< $(CFLAGS) -DUSE_POSIX

grep-ff-tre.o: grep-ff-single.c
	gcc -c -o $@ $< $(CFLAGS) -DUSE_TRE


grep-ff-frec: grep-ff-frec.o
	gcc -o $@ $< -L../../libfrec/lib -lfrec -ltre

grep-ff-posix: grep-ff-posix.o
	gcc -o $@ $<

grep-ff-tre: grep-ff-tre.o
	gcc -o $@ $< -ltre
