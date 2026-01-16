CC := gcc
STD := -std=c17
SRC := src/
INC := include/
PFLAGS := -I$(INC)
CFLAGS := -Wall -O2 -fPIC
LFLAGS := -shared


$(LIB)libcodec.so: codec.o
	$(CC) $(LFLAGS) $< -o $@

%.o : $(SRC)%.c
	$(CC) -c $(STD) $(PFLAGS) $(CFLAGS) $< -o $@

all: $(LIB)libcodec.so

clean:
	rm -f $(LIB)*.o $(LIB)*.so