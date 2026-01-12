# Makefile minimaliste : construit libdif.so et diftool
CC = gcc
CFLAGS = -Wall -g -fPIC
LIBDIR = CoDec
LIBSRC = $(LIBDIR)/src/codec.c
LIBOBJ = $(LIBDIR)/codec.o
LIB = $(LIBDIR)/libdif.so
TARGET = dif

all: $(LIB) $(TARGET)

$(LIB): $(LIBOBJ)
	$(CC) -shared -o $@ $^

$(LIBOBJ): $(LIBSRC)
	$(CC) $(CFLAGS) -I$(LIBDIR)/include -c $< -o $@

$(TARGET): main.c $(LIB)
	$(CC) $(CFLAGS) -I$(LIBDIR)/include main.c -L$(LIBDIR) -ldif -Wl,-rpath,'$$ORIGIN/CoDec' -o $@

clean:
	rm -f $(LIBOBJ) $(LIB) $(TARGET)

.PHONY: all clean
