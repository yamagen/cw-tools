CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic
LDLIBS ?= -lm

PROGS = pair cw emit

all: $(PROGS)

pair: src/pair.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

cw: src/cw.o src/common.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

EMIT_OBJS = src/emit.o src/emit-util.o src/emit-json.o src/emit-dot.o \
            src/emit-tables.o src/emit-d3.o

emit: $(EMIT_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(PROGS) src/*.o
