CC = g++
CFLAGS = -Wall -pthread
SOURCES = main.cpp Wisielec.cpp
EXECUTABLE = wisielec.out

all: $(EXECUTABLE)

$(EXECUTABLE): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(EXECUTABLE)