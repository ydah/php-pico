CC ?= cc
CPPFLAGS := -Iinclude -Isrc -Icompiler -Istdlib -Itools -DPPHP_HOST=1
CFLAGS := -std=c99 -Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow -O2
LDFLAGS :=
LDLIBS := -lm

CORE_SOURCES := src/alloc.c src/value.c src/pstring.c src/symbol.c src/parray.c src/resource.c src/pclass.c src/closure.c
COMPILER_SOURCES := compiler/lexer.c compiler/ast.c compiler/parser.c
RUNTIME_SOURCES := $(CORE_SOURCES) src/exception.c src/value_ops.c src/pbc.c src/state.c src/vm.c stdlib/builtins.c stdlib/strings.c stdlib/arrays.c stdlib/formatting.c stdlib/json.c stdlib/system.c hal/posix/hal_posix.c
HOST_SOURCES := $(RUNTIME_SOURCES) $(COMPILER_SOURCES) compiler/codegen.c tools/disasm.c ports/host/main.c
TEST_SOURCES := $(CORE_SOURCES) tests/unit/test_core.c
LEXER_TEST_SOURCES := compiler/lexer.c tests/unit/test_lexer.c
PARSER_TEST_SOURCES := src/alloc.c compiler/lexer.c compiler/ast.c compiler/parser.c tests/unit/test_parser.c
VM_TEST_SOURCES := $(RUNTIME_SOURCES) $(COMPILER_SOURCES) compiler/codegen.c tests/unit/test_vm.c
HOST_BINARY := build/host/php-pico
TEST_BINARY := build/host/test_core
LEXER_TEST_BINARY := build/host/test_lexer
PARSER_TEST_BINARY := build/host/test_parser
VM_TEST_BINARY := build/host/test_vm
ASAN_BINARY := build/host/test_core_asan
ASAN_LEXER_BINARY := build/host/test_lexer_asan
ASAN_PARSER_BINARY := build/host/test_parser_asan
ASAN_VM_BINARY := build/host/test_vm_asan
ASAN_LEAKS := $(if $(filter Darwin,$(shell uname -s)),0,1)

.PHONY: all host test test-asan test-diff size clean

all: host

host: $(HOST_BINARY)

$(HOST_BINARY): $(HOST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(HOST_SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_BINARY): $(TEST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TEST_SOURCES) $(LDFLAGS) -o $@

$(LEXER_TEST_BINARY): $(LEXER_TEST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LEXER_TEST_SOURCES) $(LDFLAGS) -o $@

$(PARSER_TEST_BINARY): $(PARSER_TEST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PARSER_TEST_SOURCES) $(LDFLAGS) -o $@

$(VM_TEST_BINARY): $(VM_TEST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(VM_TEST_SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

test: $(TEST_BINARY) $(LEXER_TEST_BINARY) $(PARSER_TEST_BINARY) $(VM_TEST_BINARY) $(HOST_BINARY)
	$(TEST_BINARY)
	$(LEXER_TEST_BINARY)
	$(PARSER_TEST_BINARY)
	$(VM_TEST_BINARY)
	$(HOST_BINARY) --version

test-asan:
	@mkdir -p $(dir $(ASAN_BINARY))
	$(CC) $(CPPFLAGS) $(CFLAGS) -O1 -g -fno-omit-frame-pointer \
		-fsanitize=address,undefined $(TEST_SOURCES) -fsanitize=address,undefined -o $(ASAN_BINARY)
	ASAN_OPTIONS=detect_leaks=$(ASAN_LEAKS) $(ASAN_BINARY)
	$(CC) $(CPPFLAGS) $(CFLAGS) -O1 -g -fno-omit-frame-pointer \
		-fsanitize=address,undefined $(LEXER_TEST_SOURCES) -fsanitize=address,undefined -o $(ASAN_LEXER_BINARY)
	ASAN_OPTIONS=detect_leaks=$(ASAN_LEAKS) $(ASAN_LEXER_BINARY)
	$(CC) $(CPPFLAGS) $(CFLAGS) -O1 -g -fno-omit-frame-pointer \
		-fsanitize=address,undefined $(PARSER_TEST_SOURCES) -fsanitize=address,undefined -o $(ASAN_PARSER_BINARY)
	ASAN_OPTIONS=detect_leaks=$(ASAN_LEAKS) $(ASAN_PARSER_BINARY)
	$(CC) $(CPPFLAGS) $(CFLAGS) -O1 -g -fno-omit-frame-pointer \
		-fsanitize=address,undefined $(VM_TEST_SOURCES) -fsanitize=address,undefined $(LDLIBS) -o $(ASAN_VM_BINARY)
	ASAN_OPTIONS=detect_leaks=$(ASAN_LEAKS) $(ASAN_VM_BINARY)

test-diff: test
	@printf 'Differential tests are added with the language runtime milestones.\n'

size: host
	sh tools/sizecheck.sh $(HOST_BINARY)

clean:
	rm -rf build
