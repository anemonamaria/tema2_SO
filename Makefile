build: libso_stdio.so

libso_stdio.so: libso.o
	gcc -Wall -shared libso.o -o libso_stdio.so

libso.o: libso.c
	gcc -Wall -fPIC -o libso.o -c libso.c

.PHONY: clean

clean:
	rm -f *.o libso_stdio.so