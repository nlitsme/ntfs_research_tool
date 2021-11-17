CMAKEARGS+=$(if $(D),-DCMAKE_BUILD_TYPE=Debug,-DCMAKE_BUILD_TYPE=Release)
CMAKEARGS+=$(if $(COV),-DOPT_COV=1)
CMAKEARGS+=$(if $(PROF),-DOPT_PROF=1)
CMAKEARGS+=$(if $(LIBCXX),-DOPT_LIBCXX=1)

all:
	cmake -B build . $(CMAKEARGS)
	$(MAKE) -C build $(if $(V),VERBOSE=1)

llvm:
	CC=clang CXX=clang++ cmake -B build . $(CMAKEARGS)
	$(MAKE) -C build $(if $(V),VERBOSE=1)



ntfsrd: ntfsrd.o utfcvutils.o stringutils.o
	$(CXX) -g -o $@ $^

%.o: %.cpp
	$(CXX) -c -Wall -O3 -g -std=c++1z -o $@ $^ -I ../../itslib/include/itslib

%.o: ../../itslib/src/%.cpp
	$(CXX) -c -Wall -O3 -g -std=c++1z -o $@ $^ -I ../../itslib/include/itslib

clean:
	$(RM) $(wildcard *.o) ntfsrd
	$(RM) -r build CMakeFiles CMakeCache.txt CMakeOutput.log

COVOPTIONS+=-show-instantiation-summary
#COVOPTIONS+=-show-functions
COVOPTIONS+=-show-expansions
COVOPTIONS+=-show-regions
COVOPTIONS+=-show-line-counts-or-regions


COVERAGEFILES=base58.h bcaddress.h bcio.h bciter.h bcmessagehash.h bcpacker.h bcscript.h bcscriptcodes.h bcunpacker.h bcvalue.h bech32.h bitcoin_line_protocol.h
COVERAGEFILES+=bitcoin_protocol.h bitcoincurve.h bitcoinmagic.h bitflags.h bloomfilter.h ec.h ecdsa.h fastsecp.h gfp.h hashing.h logger.h mpzutils.h olll.h pointencoder.h
COVERAGEFILES+=primes.h scriptclassifier.h spanutils.h transactionhash.h eth_rlpx_protocol.h crypto_utils.h ciphers.h ethio.h

coverage:
	llvm-profdata merge -o unittest.profdata default.profraw
	llvm-cov show ./build/unittests -instr-profile=unittest.profdata $(COVOPTIONS) $(COVERAGEFILES)

gccprofile:
	build/bccrack  h.52
	gprof build/bccrack

callgrind:
	valgrind --tool=callgrind --dump-instr=yes --simulate-cache=yes --collect-jumps=yes build/bccrack  h.52
	# callgrind_annotate

profile:
	# todo - how to build the corresponding binary?
	llvm-profdata merge -output=merge.out -instr default.profraw
	llvm-cov show build/bccrack  --instr-profile=merge.out
	llvm-profdata show --topn=100 -instr default.profraw
