ntfsrd: ntfsrd.o utfcvutils.o stringutils.o
	$(CXX) -g -o $@ $^

%.o: %.cpp
	$(CXX) -c -Wall -O3 -g -std=c++1z -o $@ $^ -I ../../itslib/include/itslib

%.o: ../../itslib/src/%.cpp
	$(CXX) -c -Wall -O3 -g -std=c++1z -o $@ $^ -I ../../itslib/include/itslib

clean:
	$(RM) $(wildcard *.o) ntfsrd
