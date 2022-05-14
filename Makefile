CC=cc
CFLAGS=-Wall -O2
LFLAGS=-lm

OBJS=stb_image.o imgfb.o

TARGET=imgfb

.PHONY: all clean

all: $(TARGET)

# Make the target depend on all the objects instead of 1 source file
$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LFLAGS)

# This rule compiles each source file (.c) into an object file (.o)
# each time the source is changed
%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -fv imgfb
