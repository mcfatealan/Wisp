.PHONY: clean
	
CC=g++
CFLAGS=-c -Wall -std=c++11
LDFLAGS=-lcityhash -lzmq -lrdmacm -libverbs -lpthread
SOURCES=src/main.cpp src/server.cpp src/client.cpp src/rdma.cpp ext/rdma_lib/rdmaio.cc ext/rdma_lib/rdma_msg.cc ext/getoptpp/getopt_pp.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=wisp

all: $(SOURCES) $(EXECUTABLE) 
    
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS) -std=c++11

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
