# Makefile to build lqjs.so from test.c and libquickjs.a

# Compiler
CC = gcc

# Compiler flags
CFLAGS = -fPIC -Wall -Wextra -O2 -Ib

# Linker flags for shared library
LDFLAGS = -shared

# Source file
#SRC = lquickjs.c
SRC = lqjs.c

# Object file
OBJ = $(SRC:.c=.o)

# Output shared library
TARGET = lqjs.so

# Third-party library
LIBS = quickjs-ng/build/libqjs.a

# Default target
all: $(TARGET)

# Link the shared library
$(TARGET): $(OBJ) $(LIBS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS) -lm -pthread

# Compile the source file to object file
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean build artifacts
clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean