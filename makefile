# The target is the name of the executable file that you want to create.
TARGET = main

# The compiler is the program that you want to use to compile the C source files.
CC = gcc

# The compiler flags are the options that you want to pass to the compiler.
CFLAGS = -std=c89 -pedantic -D_GNU_SOURCE -g 

# The linker is the program that you want to use to link the object files together.
LD = gcc

default : all

# The rule for compiling a C source file.
main: main.o nave porto
	$(CC) $(CFLAGS) -o bin/main bin/main.o

main.o: src/main.c lib/lib_so.h
	$(CC) $(CFLAGS) -c src/main.c -o bin/main.o

nave: nave.o 
	$(CC) $(CFLAGS) -o bin/nave bin/nave.o -lm

nave.o: src/nave.c lib/lib_so.h
	$(CC) $(CFLAGS) -c src/nave.c -o bin/nave.o  

porto: porto.o
	$(CC) $(CFLAGS) -o bin/porto bin/porto.o -lm

porto.o: src/porto.c lib/lib_so.h
	$(CC) $(CFLAGS) -c src/porto.c -o bin/porto.o

meteo: meteo.o 
	$(CC) $(CFLAGS) -o bin/meteo bin/meteo.o -lm

meteo.o: src/meteo.c lib/lib_so.h
	$(CC) $(CFLAGS) -c src/meteo.c -o bin/meteo.o

# The rule for cleaning up the project.
clean:
	rm -f bin/main bin/nave bin/porto bin/meteo bin/main.o bin/nave.o bin/porto.o bin/meteo.o

# The rule for running the program.
run: all
	./bin/$(TARGET) UC

# The default target is to build the executable file.
.PHONY: all
all: main nave porto meteo