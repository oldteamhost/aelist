FLAGS=-lncurses -ltinfo -lpanel -O3 -g -Wall

all:
	gcc aelist.c -c $(FLAGS)
	gcc aelist.o -o aelist $(FLAGS)
clean:
	rm aelist.o aelist
