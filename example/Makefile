
LIB = ../lib
CUDA = /usr/local/cuda

all: test.c
	gcc-4.4 -I$(LIB) -I$(CUDA)/include -L$(CUDA)/lib -lcudart test.c $(LIB)/virginian.a -o test
	./test

clean:
	rm -rf test

