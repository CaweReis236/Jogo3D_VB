# Makefile para Raylib compilada estaticamente em Linux Mint/X11
CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra
RAYLIB = $(shell pkg-config --cflags --libs raylib)
# A biblioteca estatica requer a linkagem explicita do X11 (apos -lraylib).
LDLIBS = -lX11 -lXrandr -lXi -lXcursor -lXinerama -lGL -lm -lpthread -ldl

all: simulador

simulador: main.c
	$(CC) $(CFLAGS) $< -o $@ $(RAYLIB) $(LDLIBS)

run: simulador
	./simulador

clean:
	rm -f simulador

.PHONY: all run clean
