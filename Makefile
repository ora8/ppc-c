CC      ?= gcc
CFLAGS  ?= -std=gnu11 -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare
BUILD   := build
SRCS    := src/util.c src/parser.c src/sema.c src/borrow.c src/codegen.c src/lsp.c src/main.c
OBJS    := $(SRCS:src/%.c=$(BUILD)/%.o) $(BUILD)/runtime.o $(BUILD)/stdlib.o

all: ppc

ppc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(BUILD)/%.o: src/%.c src/ast.h src/util.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# embed the C runtime as a string literal
$(BUILD)/runtime.c: src/ppc_rt.h | $(BUILD)
	@echo 'const char *PPC_RUNTIME =' > $@
	@sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/\\n"/' $< >> $@
	@echo ';' >> $@

$(BUILD)/runtime.o: $(BUILD)/runtime.c
	$(CC) $(CFLAGS) -c $< -o $@

# embed the standard library (Perfect Pascal source) for 'uses ppc_std'
$(BUILD)/stdlib.c: lib/ppc_std.pas | $(BUILD)
	@echo 'const char *PPC_STD =' > $@
	@sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/\\n"/' $< >> $@
	@echo ';' >> $@

$(BUILD)/stdlib.o: $(BUILD)/stdlib.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

test: ppc
	./tests/run.sh

clean:
	rm -rf $(BUILD) ppc

.PHONY: all clean test
