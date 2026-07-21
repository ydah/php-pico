CC ?= cc
CPPFLAGS := -Iinclude -Isrc -Icompiler -DPPHP_HOST=1
CFLAGS := -std=c99 -Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow -O2
LDFLAGS :=

CORE_SOURCES := src/alloc.c src/value.c src/pstring.c src/symbol.c
HOST_SOURCES := $(CORE_SOURCES) ports/host/main.c
TEST_SOURCES := $(CORE_SOURCES) tests/unit/test_core.c
HOST_BINARY := build/host/php-pico
TEST_BINARY := build/host/test_core
ASAN_BINARY := build/host/test_core_asan
ASAN_LEAKS := $(if $(filter Darwin,$(shell uname -s)),0,1)

.PHONY: all host test test-asan test-diff size clean

all: host

host: $(HOST_BINARY)

$(HOST_BINARY): $(HOST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(HOST_SOURCES) $(LDFLAGS) -o $@

$(TEST_BINARY): $(TEST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TEST_SOURCES) $(LDFLAGS) -o $@

test: $(TEST_BINARY) $(HOST_BINARY)
	$(TEST_BINARY)
	$(HOST_BINARY) --version

test-asan:
	@mkdir -p $(dir $(ASAN_BINARY))
	$(CC) $(CPPFLAGS) $(CFLAGS) -O1 -g -fno-omit-frame-pointer \
		-fsanitize=address,undefined $(TEST_SOURCES) -fsanitize=address,undefined -o $(ASAN_BINARY)
	ASAN_OPTIONS=detect_leaks=$(ASAN_LEAKS) $(ASAN_BINARY)

test-diff: test
	@printf 'Differential tests are added with the language runtime milestones.\n'

size: host
	sh tools/sizecheck.sh $(HOST_BINARY)

clean:
	rm -rf build
