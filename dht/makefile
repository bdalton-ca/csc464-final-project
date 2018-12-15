# Variables
CC=g++
STD=c++11
CFLAGS= -std=$(STD) -Wno-write-strings -c -g
LDFLAGS= -lpthread 
SRC= $(wildcard src/*.c)
HDR= $(wildcard src/*.h)
OBJ= $(patsubst src/%.c,obj/%.o,$(SRC)) 


# Rules


all: $(SRC) $(OBJ)
	$(CC) -o main $(OBJ) $(LDFLAGS)

obj/%.o: src/%.c $(HDR)
	$(CC) $(CFLAGS) $< -o $@
	
