CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra $(shell pkg-config --cflags raylib)
LDLIBS = $(shell pkg-config --libs raylib) -lX11 -lXrandr -lXi -lXcursor -lXinerama -lGL -lm -lpthread -ldl

all: simulador

simulador: main.c
	$(CC) $(CFLAGS) main.c -o simulador $(LDLIBS)

run: simulador
	./simulador

clean:
	rm -f simulador

.PHONY: all run clean
