
# set config to debug, gccrelease, or iccrelease
CONFIG = gccrelease

# file with main function
ENTRY_POINT = 

# custom compiler declarations
CUSTOM_FLAGS = #-D VIRG_DEBUG #-D VIRG_NOPINNED #-D VIRG_DEBUG_SLOTS

GCC = /usr/bin/gcc
GPP = /usr/bin/g++-4.4
ICC = /opt/intel/Compiler/11.1/064/bin/intel64/icc
CUDA = /usr/local/cuda
NVCC = $(CUDA)/bin/nvcc
CUDA_INCLUDE = $(CUDA)/include
CUDA_LIBRARY = $(CUDA)/lib
GTEST = ../gtest

ifeq ($(CONFIG),iccrelease)
	FLAGS = -Wall -w1 -std=c99
else
	FLAGS = -Wall -Wextra -std=c99
endif

INCLUDE_FLAGS = -I $(CUDA_INCLUDE) -I . -I sql/
LINK_FLAGS = -lm -lpthread -L$(CUDA_LIBRARY) -lcuda -lcudart
DEBUG_FLAGS = -g3
RELEASE_FLAGS = -O2
FLAG = $(FLAGS) $(CUSTOM_FLAGS) $(INCLUDE_FLAGS)
CPPFLAG = -Wall $(CUSTOM_FLAGS) $(INCLUDE_FLAGS)

ifeq ($(CONFIG),debug)
	OF = debug
	CC = $(GCC)
	COMPILE_FLAGS = $(FLAG) $(DEBUG_FLAGS)
endif
ifeq ($(CONFIG),gccrelease)
	OF = release.gcc
	CC = $(GCC)
	COMPILE_FLAGS = $(FLAG) $(RELEASE_FLAGS)
endif
ifeq ($(CONFIG),iccrelease)
	OF = release.icc
	CC = $(ICC)
	COMPILE_FLAGS = $(FLAG) $(RELEASE_FLAGS)
endif

SPECIALS = opcodelist.c sql/lex.yy.c sql/sql.tab.c
CFILES = $(filter-out $(SPECIALS),$(wildcard *.c) $(wildcard */*.c) $(ENTRY_POINT)) $(SPECIALS)
OFILES = $(patsubst %.c,../$(OF)/%.o,$(CFILES))
LFILES = $(OFILES) ../$(OF)/vm/virginia-single.o ../$(OF)/vm/vm_gpu.o

TESTCFILES = $(wildcard test/*.cc)
TESTOFILES = $(patsubst %.cc,../$(OF)/%.o,$(TESTCFILES))

virginian: ../lib/virginian.a $(ENTRY_POINT) test ../$(OF)/virginiantest ../db/comparedb
	cd ../$(OF) && ./virginiantest
	#cd ../$(OF) && valgrind --leak-check=full --suppressions=/home/bakks/virginian/virginian/src/cuda.supp ./virginiantest
#  --gen-suppressions=yes

clean:
	rm -rf ../debug/* ../release.gcc/* ../release.icc/* opcodelist.c vm_gpu.ptx sql/lex.yy.c sql/sql.tab.*
	rm -rf ../lib/virginian.*
	ls -d */ | sed 's/[a-z]*\//..\/debug\/\0/' | xargs mkdir
	ls -d */ | sed 's/[a-z]*\//..\/release.gcc\/\0/' | xargs mkdir
	ls -d */ | sed 's/[a-z]*\//..\/release.icc\/\0/' | xargs mkdir

count:
	wc -l *.c *.h */*.c */*.cu */*.h */*.cc */*.y */*.l


$(OFILES): ../$(OF)/%.o: %.c virginian.h
	$(CC) $(COMPILE_FLAGS) -c $< -o $@

../$(OF)/vm/virginia-single.o: vm/virginia.c virginian.h
	$(CC) $(COMPILE_FLAGS) -D __SINGLE -c $< -o $@

../$(OF)/vm/vm_gpu.o: vm/vm_gpu.cu virginian.h
	$(NVCC) $(CUSTOM_FLAGS) $(INCLUDE_FLAGS) -O2 -arch=sm_20 -c $< -o $@

opcodelist.c: opcodelist.awk virginian.h
	./opcodelist.awk virginian.h > opcodelist.c

../$(OF)/virginian: $(LFILES)
	$(CC) $(LFILES) $(LINK_FLAGS) -o ../$(OF)/virginian

../lib/virginian.a: $(LFILES)
	ar rvs ../lib/virginian.a $(LFILES)
	cp virginian.h ../lib

../db/comparedb:
	make -s -C ../db clean
	make -s -C ../db
	export LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):/opt/intel/Compiler/11.1/064/lib/intel64 && \
	../db/generate ../db/comparedb 8000000

ptx: vm/vm_gpu.cu
	$(NVCC) $(CUSTOM_FLAGS) $(INCLUDE_FLAGS) --ptxas-options="-v" -arch=sm_20 -g -ptx $< -o vm_gpu.ptx

$(TESTOFILES): ../$(OF)/%.o: %.cc virginian.h test/test.h
	$(GPP) -g3 -I../lib $(CPPFLAG) -c $< -o $@

../$(OF)/virginiantest: $(LFILES) $(TESTOFILES) ../lib/virginian.a
	$(GPP) $(LINK_FLAGS) $(TESTOFILES) ../lib/virginian.a ../lib/libgtest.a -lstdc++ -pthread -L../lib -lgsl -lgslcblas -o ../$(OF)/virginiantest

sql/sql.tab.c: sql/sql.y sql/node.h
	bison --defines=sql/sql.tab.h -o sql/sql.tab.c sql/sql.y

sql/sql.tab.h: sql/sql.y sql/node.h
	bison --defines=sql/sql.tab.h -o sql/sql.tab.c sql/sql.y

sql/lex.yy.c: sql/sql.l sql/sql.tab.h sql/node.h
	flex -o sql/lex.yy.c sql/sql.l





.PHONY: virginian
.PHONY: clean
.PHONY: count

