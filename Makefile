CC=cc
CFLAGS=-Wall -O2
LFLAGS=-lm

TARGET=imgfb

.PHONY: all clean

all: $(TARGET)

$(TARGET): imgfb.c
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

clean:
	rm -fv imgfb
