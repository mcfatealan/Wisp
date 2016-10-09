.PHONY: clean
	
CC=g++
CPPFLAGS=-c -Wall -std=c++11
CCFLAGS=-c -std=c++11
LDFLAGS=-lcityhash -lzmq -lrdmacm -libverbs -lpthread
CPP_SOURCES=src/main.cpp src/server.cpp src/client.cpp src/rdma.cpp ext/getoptpp/getopt_pp.cpp
CC_SOURCES=ext/rdma_lib/rdmaio.cc ext/rdma_lib/rdma_msg.cc
SOURCES=$(CPP_SOURCES) $(CC_SOURCES)
OBJECTS=$(CPP_SOURCES:.cpp=.o) $(CC_SOURCES:.cc=.o)
EXECUTABLE=wisp

all: $(SOURCES) $(EXECUTABLE) 
    
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS) -std=c++11

.cpp.o:
	$(CC) $(CPPFLAGS) $< -o $@

.cc.o:
	$(CC) $(CCFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
