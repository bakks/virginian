GSL_VER = 1.14

all:
	make -C src

clean:
	rm -rf gsl gsl-$(GSL_VER) db/comparedb virginian.tar.gz debug release
	mkdir -p doc
	make -C example clean
	make -C db clean
	make -C src clean

gsl:
	rm -rf gsl-$(GSL_VER) gsl lib/libgsl.so lib/libgslcblas.so lib/gsl
	wget ftp://ftp.gnu.org/gnu/gsl/gsl-$(GSL_VER).tar.gz
	tar -xzzvf gsl-$(GSL_VER).tar.gz
	rm gsl-$(GSL_VER).tar.gz
	mkdir gsl
	cd gsl && ../gsl-$(GSL_VER)/configure
	cd gsl && make
	cp gsl/cblas/.libs/libgslcblas.so lib
	cp gsl/.libs/libgsl.so lib
	cp -rL gsl/gsl/ lib/
	rm -rf gsl-$(GSL_VER) gsl lib/gsl/Makefile

gtest:
	rm -rf lib/libgtest.a lib/gtest
	wget http://googletest.googlecode.com/files/gtest-1.6.0.zip
	unzip gtest-1.6.0.zip
	mv gtest-1.6.0 gtest
	rm gtest-1.6.0.zip
	g++ -g3 -Igtest/include -Igtest -c gtest/src/gtest-all.cc -o lib/gtest-all.o
	ar -rv lib/libgtest.a lib/gtest-all.o
	cp -rL gtest/include/gtest lib/
	rm lib/gtest-all.o
	rm -rf gtest

package:
	make -C src clean
	tar czvf virginian.tar.gz ../virginian/db/Makefile ../virginian/db/generate.c ../virginian/lib/ ../virginian/debug/ ../virginian/release.gcc/ ../virginian/release.icc/ ../virginian/doc/ ../virginian/src/ ../virginian/example/ ../virginian/Makefile ../virginian/README

.PHONY: package
.PHONY: gsl
.PHONY: gtest

