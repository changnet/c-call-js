CC=gcc
CFLAGS=-I./quickjs-ng -g
LDFLAGS=-L./quickjs-ng/build -lqjs -lm -lpthread -ldl

TARGET=main

all: $(TARGET)

$(TARGET): main.o
	$(CC) -o $@ $^ $(LDFLAGS)

main.o: main.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean