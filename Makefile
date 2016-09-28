CC=g++
CFLAGS=-c -Wall -std=c++11 
LDFLAGS=-lcityhash -lzmq
SOURCES=src/main.cpp src/server.cpp src/client.cpp ext/getoptpp/getopt_pp.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=wisp

all: $(SOURCES) $(EXECUTABLE) clean
    
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm $(OBJECTS)
