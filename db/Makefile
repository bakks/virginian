
generate: generate.c ../lib/virginian.a ../src/virginian.h
	gcc -O2 -Wall -Wextra -pedantic -std=c99 -I ../src -I ../lib -I /usr/local/cuda/include -c generate.c -o generate.o
	gcc generate.o ../lib/virginian.a -lgsl -lgslcblas -lcudart -L ../lib -L /usr/local/cuda/lib -o generate

../lib/virginian.a: ../src/Makefile
	make -C ../src ../lib/virginian.a

clean:
	rm -f generate generate.o

.PHONY: clean

