OS := $(shell uname)
ARCH := $(shell uname -m)

LIBJIT_INCLUDE := -Ilibjit-install/include

ifeq ($(OS), Darwin)
LIBJIT_LIB := -Llibjit-install/lib/$(ARCH)/
else
LIBJIT_LIB := -Llibjit-install/lib
endif

DEBUG_FLAGS = -ggdb
OPTIMIZE = -O3

SOURCES   := $(wildcard *.c)
OBJECTS   := $(patsubst %.c, %.o, $(wildcard *.c))

CFLAGS  += -I. $(OPTIMIZE) $(DEBUG_FLAGS) -pthread $(LIBJIT_INCLUDE)
LDFLAGS += $(LIBJIT_LIB) -ljit -lm

all: jit_test

jit_test: $(OBJECTS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

regen: pj_type_switch.h

pj_type_switch.h:
	perl make_function_invoker.pl 20 > $@

clean:
	rm -f *.o *~ core jit_test

.PHONY: clean regen
