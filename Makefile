CC = clang
CFLAGS = -O2 -Wall
PREFIX = /usr/local

UNAME := $(shell uname)

ifeq ($(UNAME), Darwin)
    LIBS = -lncurses
else
    CC = gcc
    LIBS = -lncurses -ltinfo
endif

TARGET = lucrativeText

all:
	$(CC) $(CFLAGS) main.c -o $(TARGET) $(LIBS)

install: all
	cp $(TARGET) $(PREFIX)/bin/$(TARGET)

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)

clean:
	rm -f $(TARGET)