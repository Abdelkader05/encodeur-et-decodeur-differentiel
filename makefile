CC := gcc
STD := -std=c17
SRC := src/
BIN := bin/
LPATH := CoDec/

PFLAGS :=  -I$(LPATH)include
CFLAGS := -Wall -O2

# répertoire où se trouve la lib. (truc.h|libtruc.so)

LFLAGS := -lm -Wl,-rpath,$(LPATH)lib -L$(LPATH) -lcodec

# Build the codec library first
.PHONY: codec

# executable quelconque utilisant uniquement libtruc.so
encodeur : codec $(BIN)main.o
	$(CC) $(BIN)main.o $(LFLAGS) -o $@

codec:
	$(MAKE) -C $(LPATH)
	
$(BIN)%.o : $(SRC)%.c
	$(CC) $< -c $(STD) $(PFLAGS) $(CFLAGS) -o $@

clean:
	rm $(BIN)main.o encodeur $(LPATH)*.o $(LPATH)*.so


