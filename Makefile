FLAGS=-lncurses -ltinfo -lpanel -O3 -g -Wall

all:
	cc aelist.c -c $(FLAGS)
	cc aelist.o -o aelist $(FLAGS)
clean:
	rm aelist.o aelist
