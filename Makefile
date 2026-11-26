CC      ?= gcc
CFLAGS  ?= -std=gnu11 -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare
BUILD   := build
PREFIX  ?= /usr/local
BINDIR  := $(DESTDIR)$(PREFIX)/bin
DOCDIR  := $(DESTDIR)$(PREFIX)/share/doc/ppc
SRCS    := src/util.c src/ini.c src/parser.c src/sema.c src/borrow.c src/codegen.c src/lsp.c src/main.c
OBJS    := $(SRCS:src/%.c=$(BUILD)/%.o) $(BUILD)/runtime.o $(BUILD)/stdlib.o

all: ppc ppm

ppc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

# ppm: the project tool (ppc.ini), calls ppc
ppm: $(BUILD)/ppm.o $(BUILD)/ini.o $(BUILD)/util.o
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD)/%.o: src/%.c src/ast.h src/util.h src/ini.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# embed the C runtime as a string literal
$(BUILD)/runtime.c: src/ppc_rt.h | $(BUILD)
	@echo 'const char *PPC_RUNTIME =' > $@
	@sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/\\n"/' $< >> $@
	@echo ';' >> $@

$(BUILD)/runtime.o: $(BUILD)/runtime.c
	$(CC) $(CFLAGS) -c $< -o $@

# embed the standard library (Perfect Pascal source) for 'uses std'
$(BUILD)/stdlib.c: lib/std.pas | $(BUILD)
	@echo 'const char *PPC_STD =' > $@
	@sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/\\n"/' $< >> $@
	@echo ';' >> $@

$(BUILD)/stdlib.o: $(BUILD)/stdlib.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

test: ppc ppm
	./tests/run.sh

# make install [PREFIX=/usr/local] [DESTDIR=...]: ppc and ppm (ppm uses the ppc next to it; std is built in)
install: ppc ppm
	install -d $(BINDIR) $(DOCDIR)
	install -m 755 ppc ppm $(BINDIR)
	install -m 644 README.md $(DOCDIR)
	install -m 644 lib/std.pas $(DOCDIR)

uninstall:
	rm -f $(BINDIR)/ppc $(BINDIR)/ppm
	rm -rf $(DOCDIR)

clean:
	rm -rf $(BUILD) ppc ppm

.PHONY: all clean test install uninstall
