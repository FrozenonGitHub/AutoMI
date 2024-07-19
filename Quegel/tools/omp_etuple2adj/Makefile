CCOMPILE=g++
CXXFLAGS=-std=c++17 -fopenmp
LDFLAGS = -lz -Wno-deprecated -O3

all: convert

convert: main.cpp
	$(CCOMPILE) main.cpp $(CXXFLAGS) $(LDFLAGS) -o convert

clean:
	-rm convert
