PROGS = pair cw emit cm

SRC_DIR = src

PAIR_OBJ = $(SRC_DIR)/pair.o
CW_OBJ  = $(SRC_DIR)/cw.o
CM_OBJ  = $(SRC_DIR)/cm.o
EMIT_OBJ  = $(SRC_DIR)/emit.o
COMMON_OBJ = src/common.o


CC       ?= cc
CPPFLAGS ?=
CFLAGS   ?= -O2 -std=c11 -Wall -Wextra -Wpedantic
LDFLAGS  ?=

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

all: $(PROGS)

pair: $(PAIR_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

cw: $(CW_OBJ) $(COMMON_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ -lm

cm: $(CM_OBJ) $(COMMON_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ -lm

emit: $(EMIT_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

install: $(PROGS)
	install -d $(DESTDIR)$(BINDIR)
	for prog in $(PROGS); do \
		install -m 755 $$prog $(DESTDIR)$(BINDIR)/$$prog; \
	done

clean:
	rm -f $(PROGS) $(PAIR_OBJ) $(CW_OBJ) $(CM_OBJ) $(COMMON_OBJ) $(EMIT_OBJ)

.PHONY: all install clean
