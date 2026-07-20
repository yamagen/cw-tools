CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic
LDLIBS ?= -lm

PROGS = pair cw emit

all: $(PROGS)

pair: src/pair.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

cw: src/cw.o src/common.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

emit: src/emit.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(PROGS) src/*.o
