CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -I./include -lpthread -g
LDFLAGS_STATIC = -L./lib -ltpool
LDFLAGS_SHARED = -L./lib -Wl,-rpath=./lib -ltpool

SRCDIR = ./src
OBJDIR = ./examples
LIBDIR = ./lib
EXEDIR = ./examples

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

.PHONY: all static dynamic test clean

all: static dynamic test

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

static: $(LIBDIR)/libtpool.a

$(LIBDIR)/libtpool.a: $(OBJECTS)
	mkdir -p $(LIBDIR)
	ar rcs $@ $^

dynamic: $(LIBDIR)/libtpool.so

$(LIBDIR)/libtpool.so: $(OBJECTS)
	mkdir -p $(LIBDIR)
	$(CC) -shared -fPIC -no-pie $(LDFLAGS_SHARED) $^ -o $@

test: $(EXEDIR)/test

$(EXEDIR)/test: $(EXEDIR)/test.o $(EXEDIR)/utils.o $(EXEDIR)/threadpool.o
	mkdir -p $(EXEDIR)
	$(CC) $(CFALGS) -g $^ -o $@ -lm

$(EXEDIR)/%.o: $(EXEDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR)/*.o $(LIBDIR)/*.a $(LIBDIR)/*.so $(EXEDIR)/*.o $(EXEDIR)/test
