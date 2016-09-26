CC=g++
CFLAGS=-c -Wall
LDFLAGS=
SOURCES=src/main.cpp src/server.cpp src/client.cpp ext/getoptpp/getopt_pp.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=wisp

all: $(SOURCES) $(EXECUTABLE) cleanup
    
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

cleanup:
	rm $(OBJECTS)
