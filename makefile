

CC=clang
CFLAGS=-Wall
SRC=src/
EXEC=main

LIBTRACK= CoDec
CPPFLAGS = -I$(LIBTRACK)/include -include codec.h
LDLIBS = $(LIBTRACK)/codec.o


%.o : $(SRC)%.c 
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@ 

$(EXEC) : $(EXEC).o
	$(CC) $(CFLAGS)  $(LDLIBS) $^ -o $@

clean : 
	rm -f *.o 
