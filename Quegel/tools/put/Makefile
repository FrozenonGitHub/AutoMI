CCOMPILE=mpic++
PLATFORM=Linux-amd64-64
CPPFLAGS= -I$(HADOOP_HOME)/src/c++/libhdfs -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux -I$(HADOOP_HOME)/include -I ../../system
#LIB = -L$(HADOOP_HOME)/c++/$(PLATFORM)/lib
LIB = -L$(HADOOP_HOME)/lib/native
LDFLAGS = -lhdfs -Wno-deprecated -O2

all: put

put: put.cpp
	$(CCOMPILE) put.cpp $(CPPFLAGS) $(LIB) $(LDFLAGS)  -o put

clean:
	-rm put
