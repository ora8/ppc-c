CC      ?= gcc
CFLAGS  ?= -std=gnu11 -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare
BUILD   := build
SRCS    := src/util.c src/parser.c src/sema.c src/borrow.c src/codegen.c src/main.c
OBJS    := $(SRCS:src/%.c=$(BUILD)/%.o) $(BUILD)/runtime.o

all: ppp

ppp: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(BUILD)/%.o: src/%.c src/ast.h src/util.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# embed the C runtime as a string literal
$(BUILD)/runtime.c: src/ppp_rt.h | $(BUILD)
	@echo 'const char *PPP_RUNTIME =' > $@
	@sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/\\n"/' $< >> $@
	@echo ';' >> $@

$(BUILD)/runtime.o: $(BUILD)/runtime.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

test: ppp
	./tests/run.sh

clean:
	rm -rf $(BUILD) ppp

.PHONY: all clean test
