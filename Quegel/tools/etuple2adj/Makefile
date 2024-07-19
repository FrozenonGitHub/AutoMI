CCOMPILE=mpic++
LDFLAGS = -lz -Wno-deprecated -O3

all: convert

convert: main.cpp
	$(CCOMPILE) main.cpp $(LDFLAGS) -o convert

clean:
	-rm convert
