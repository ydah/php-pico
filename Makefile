CC ?= cc
PPHP_ENABLE_COMPILER ?= 1
PPHP_ENABLE_FLOAT ?= 1
PPHP_WARNINGS ?= 1
PPHP_TYPECHECK ?= 0
PPHP_RC_DEBUG ?= 0
ifeq ($(strip $(PPHP_ENABLE_FLOAT)),)
$(error PPHP_ENABLE_FLOAT must be 0 or 1)
endif
ifneq ($(filter $(PPHP_ENABLE_FLOAT),0 1),$(PPHP_ENABLE_FLOAT))
$(error PPHP_ENABLE_FLOAT must be 0 or 1)
endif
ifeq ($(strip $(PPHP_WARNINGS)),)
$(error PPHP_WARNINGS must be 0 or 1)
endif
ifneq ($(filter $(PPHP_WARNINGS),0 1),$(PPHP_WARNINGS))
$(error PPHP_WARNINGS must be 0 or 1)
endif
ifeq ($(strip $(PPHP_TYPECHECK)),)
$(error PPHP_TYPECHECK must be 0 or 1)
endif
ifneq ($(filter $(PPHP_TYPECHECK),0 1),$(PPHP_TYPECHECK))
$(error PPHP_TYPECHECK must be 0 or 1)
endif
ifeq ($(strip $(PPHP_RC_DEBUG)),)
$(error PPHP_RC_DEBUG must be 0 or 1)
endif
ifneq ($(filter $(PPHP_RC_DEBUG),0 1),$(PPHP_RC_DEBUG))
$(error PPHP_RC_DEBUG must be 0 or 1)
endif
BASE_CPPFLAGS := -Iinclude -Isrc -Istdlib -Itools -Ipgems -DPPHP_HOST=1 -DPPHP_ENABLE_FLOAT=$(PPHP_ENABLE_FLOAT) -DPPHP_WARNINGS=$(PPHP_WARNINGS) -DPPHP_TYPECHECK=$(PPHP_TYPECHECK) -DPPHP_RC_DEBUG=$(PPHP_RC_DEBUG)
COMPILER_CPPFLAGS := $(BASE_CPPFLAGS) -Icompiler -Ishell -DPPHP_ENABLE_COMPILER=1
PBC_CPPFLAGS := $(BASE_CPPFLAGS) -DPPHP_ENABLE_COMPILER=0
ifeq ($(PPHP_ENABLE_COMPILER),1)
CPPFLAGS := $(COMPILER_CPPFLAGS)
else ifeq ($(PPHP_ENABLE_COMPILER),0)
CPPFLAGS := $(PBC_CPPFLAGS)
else
$(error PPHP_ENABLE_COMPILER must be 0 or 1)
endif
CFLAGS := -std=c99 -Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow -O2
LDFLAGS :=
ifeq ($(PPHP_ENABLE_FLOAT),1)
LDLIBS := -lm
else
LDLIBS :=
endif

CORE_SOURCES := src/alloc.c src/value.c src/pstring.c src/symbol.c src/parray.c src/resource.c src/module.c src/pclass.c src/closure.c
COMPILER_SOURCES := compiler/lexer.c compiler/ast.c compiler/parser.c
FLOAT_SOURCES := $(if $(filter 1,$(PPHP_ENABLE_FLOAT)),src/float_format.c)
RUNTIME_COMMON_SOURCES := $(CORE_SOURCES) src/api.c src/exception.c src/gc.c src/rc_debug.c src/value_ops.c src/pbc.c src/state.c src/vm.c stdlib/builtins.c stdlib/strings.c stdlib/arrays.c stdlib/formatting.c stdlib/json.c stdlib/system.c stdlib/files.c pgems/pgems.c fs/fs_posix.c hal/posix/hal_posix.c
RUNTIME_SOURCES := $(RUNTIME_COMMON_SOURCES) $(FLOAT_SOURCES)
COMPILER_HOST_SOURCES := $(RUNTIME_SOURCES) $(COMPILER_SOURCES) compiler/codegen.c tools/disasm.c shell/p2sh.c shell/p2sh_device.c ports/host/main.c
PBC_HOST_SOURCES := $(RUNTIME_SOURCES) tools/disasm.c ports/host/main.c
ifeq ($(PPHP_ENABLE_COMPILER),1)
HOST_SOURCES := $(COMPILER_HOST_SOURCES)
else
HOST_SOURCES := $(PBC_HOST_SOURCES)
endif
COMPILER_OFF_TEST_SOURCES := $(RUNTIME_SOURCES) tests/unit/test_compiler_off.c
TEST_SOURCES := $(CORE_SOURCES) src/gc.c tests/unit/test_core.c
LEXER_TEST_SOURCES := compiler/lexer.c tests/unit/test_lexer.c
PARSER_TEST_SOURCES := src/alloc.c compiler/lexer.c compiler/ast.c compiler/parser.c tests/unit/test_parser.c
VM_TEST_SOURCES := $(RUNTIME_SOURCES) $(COMPILER_SOURCES) compiler/codegen.c tests/unit/test_vm.c
RC_DEBUG_TEST_SOURCES := $(RUNTIME_SOURCES) $(COMPILER_SOURCES) compiler/codegen.c tests/unit/test_rc_debug.c
HOST_BINARY := build/host/php-pico
PBC_HOST_BINARY := build/host/php-pico-pbc
CONFIGURED_PBC_HOST_BINARY := build/host/php-pico-config-off
COMPILER_OFF_TEST_BINARY := build/host/test_compiler_off
COMPILER_OFF_DEVICE_OBJECT := build/host/p2sh_device_compiler_off.o
NO_FLOAT_HOST_BINARY := build/host/php-pico-no-float
NO_FLOAT_PBC_HOST_BINARY := build/host/php-pico-no-float-pbc
NO_FLOAT_UBSAN_BINARY := build/host/php-pico-no-float-ubsan
NO_FLOAT_INT64_BINARY := build/host/php-pico-no-float-int64
NO_FLOAT_INT64_PBC_BINARY := build/host/php-pico-no-float-int64-pbc
FLOAT_INT64_BINARY := build/host/php-pico-float-int64
FLOAT_REFERENCE_BINARY := build/host/php-pico-default-float
RP2040_HOST_BINARY := build/host/php-pico-rp2040
RP2040_UBSAN_BINARY := build/host/php-pico-rp2040-ubsan
FLOAT_FORMAT_TEST_BINARY := build/host/test_float_format
WARNINGS_ON_HOST_BINARY := build/host/php-pico-warnings-on
WARNINGS_OFF_HOST_BINARY := build/host/php-pico-warnings-off
WARNINGS_ON_PBC_BINARY := build/host/php-pico-warnings-pbc-on
WARNINGS_OFF_PBC_BINARY := build/host/php-pico-warnings-pbc-off
WARNINGS_NO_FLOAT_BINARY := build/host/php-pico-warnings-no-float
WARNINGS_RP_BINARY := build/host/php-pico-warnings-rp
WARNINGS_UBSAN_BINARY := build/host/php-pico-warnings-ubsan
WARNINGS_CONFIG_ON_OBJECT := build/host/test_warnings_config_on.o
WARNINGS_CONFIG_OFF_OBJECT := build/host/test_warnings_config_off.o
WARNINGS_CONFIG_INVALID_OBJECT := build/host/test_warnings_config_invalid.o
WARNINGS_CONFIG_INVALID_LOG := build/host/test_warnings_config_invalid.log
TYPECHECK_ON_HOST_BINARY := build/host/php-pico-typecheck-on
TYPECHECK_OFF_HOST_BINARY := build/host/php-pico-typecheck-off
TYPECHECK_ON_PBC_BINARY := build/host/php-pico-typecheck-pbc-on
TYPECHECK_OFF_PBC_BINARY := build/host/php-pico-typecheck-pbc-off
TYPECHECK_NO_FLOAT_BINARY := build/host/php-pico-typecheck-no-float
TYPECHECK_NO_FLOAT_OFF_BINARY := build/host/php-pico-typecheck-no-float-off
TYPECHECK_INT64_BINARY := build/host/php-pico-typecheck-int64
TYPECHECK_RP_BINARY := build/host/php-pico-typecheck-rp
TYPECHECK_UBSAN_BINARY := build/host/php-pico-typecheck-ubsan
TYPECHECK_ASAN_BINARY := build/host/php-pico-typecheck-asan
TYPECHECK_VM_TEST_BINARY := build/host/test_vm_typecheck
TYPECHECK_VM_UBSAN_BINARY := build/host/test_vm_typecheck_ubsan
TYPECHECK_VM_ASAN_BINARY := build/host/test_vm_typecheck_asan
TYPECHECK_CONFIG_ON_OBJECT := build/host/test_typecheck_config_on.o
TYPECHECK_CONFIG_OFF_OBJECT := build/host/test_typecheck_config_off.o
TYPECHECK_CONFIG_INVALID_OBJECT := build/host/test_typecheck_config_invalid.o
TYPECHECK_CONFIG_INVALID_LOG := build/host/test_typecheck_config_invalid.log
RC_DEBUG_ON_HOST_BINARY := build/host/php-pico-rc-debug-on
RC_DEBUG_OFF_HOST_BINARY := build/host/php-pico-rc-debug-off
RC_DEBUG_NO_FLOAT_BINARY := build/host/php-pico-rc-debug-no-float
RC_DEBUG_COMPILER_OFF_BINARY := build/host/php-pico-rc-debug-compiler-off
RC_DEBUG_TEST_BINARY := build/host/test_rc_debug
RC_DEBUG_UBSAN_BINARY := build/host/test_rc_debug_ubsan
RC_DEBUG_ASAN_BINARY := build/host/test_rc_debug_asan
RC_DEBUG_TYPECHECK_BINARY := build/host/test_rc_debug_typecheck
RC_DEBUG_CONFIG_ON_OBJECT := build/host/test_rc_debug_config_on.o
RC_DEBUG_CONFIG_OFF_OBJECT := build/host/test_rc_debug_config_off.o
RC_DEBUG_CONFIG_INVALID_OBJECT := build/host/test_rc_debug_config_invalid.o
RC_DEBUG_CONFIG_INVALID_LOG := build/host/test_rc_debug_config_invalid.log
RC_DEBUG_VISITOR_API_OBJECT := build/host/test_rc_debug_visitor_api.o
TEST_BINARY := build/host/test_core
LEXER_TEST_BINARY := build/host/test_lexer
PARSER_TEST_BINARY := build/host/test_parser
VM_TEST_BINARY := build/host/test_vm
ASAN_BINARY := build/host/test_core_asan
ASAN_LEXER_BINARY := build/host/test_lexer_asan
ASAN_PARSER_BINARY := build/host/test_parser_asan
ASAN_VM_BINARY := build/host/test_vm_asan
ASAN_LEAKS := $(if $(filter Darwin,$(shell uname -s)),0,1)

.PHONY: all FORCE host host-pbc host-rp2040 rp2040 test test-unit test-compiler-off test-no-float test-no-float-ubsan test-float-format test-rp-integer-boundary test-warnings test-warnings-config test-typecheck test-typecheck-config test-rc-debug test-rc-debug-config test-phpt test-target test-asan test-diff bench size clean

FORCE:

all: host

host: $(HOST_BINARY)

host-pbc: $(PBC_HOST_BINARY)

host-rp2040: $(RP2040_HOST_BINARY)

rp2040:
	cmake -S ports/rp2040 -B build/rp2040 -DPICO_BOARD=pico
	cmake --build build/rp2040 --parallel 2

$(HOST_BINARY): FORCE $(HOST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(HOST_SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

$(PBC_HOST_BINARY): FORCE $(PBC_HOST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(PBC_CPPFLAGS) $(CFLAGS) \
		$(PBC_HOST_SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

$(CONFIGURED_PBC_HOST_BINARY): FORCE $(PBC_HOST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(PBC_CPPFLAGS) $(CFLAGS) \
		$(PBC_HOST_SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

$(COMPILER_OFF_TEST_BINARY): FORCE $(COMPILER_OFF_TEST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(PBC_CPPFLAGS) $(CFLAGS) \
		$(COMPILER_OFF_TEST_SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

$(COMPILER_OFF_DEVICE_OBJECT): FORCE shell/p2sh_device.c
	@mkdir -p $(@D)
	$(CC) $(PBC_CPPFLAGS) $(CFLAGS) -c shell/p2sh_device.c -o $@

$(NO_FLOAT_HOST_BINARY): FORCE $(RUNTIME_COMMON_SOURCES) $(COMPILER_SOURCES) compiler/codegen.c tools/disasm.c shell/p2sh.c shell/p2sh_device.c ports/host/main.c
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_ENABLE_FLOAT=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_ENABLE_FLOAT=0 $(CFLAGS) $(RUNTIME_COMMON_SOURCES) \
		$(COMPILER_SOURCES) compiler/codegen.c tools/disasm.c shell/p2sh.c \
		shell/p2sh_device.c ports/host/main.c $(LDFLAGS) -o $@

$(NO_FLOAT_PBC_HOST_BINARY): FORCE $(RUNTIME_COMMON_SOURCES) tools/disasm.c ports/host/main.c
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_ENABLE_FLOAT=%,$(PBC_CPPFLAGS)) \
		-DPPHP_ENABLE_FLOAT=0 $(CFLAGS) $(RUNTIME_COMMON_SOURCES) \
		tools/disasm.c ports/host/main.c $(LDFLAGS) -o $@

$(NO_FLOAT_UBSAN_BINARY): FORCE $(RUNTIME_COMMON_SOURCES) $(COMPILER_SOURCES) compiler/codegen.c tools/disasm.c shell/p2sh.c shell/p2sh_device.c ports/host/main.c
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_ENABLE_FLOAT=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_ENABLE_FLOAT=0 $(CFLAGS) -O1 -g -fno-omit-frame-pointer \
		-fsanitize=undefined $(RUNTIME_COMMON_SOURCES) $(COMPILER_SOURCES) \
		compiler/codegen.c tools/disasm.c shell/p2sh.c shell/p2sh_device.c \
		ports/host/main.c -fsanitize=undefined -o $@

$(NO_FLOAT_INT64_BINARY): FORCE $(RUNTIME_COMMON_SOURCES) $(COMPILER_SOURCES) compiler/codegen.c tools/disasm.c shell/p2sh.c shell/p2sh_device.c ports/host/main.c
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_ENABLE_FLOAT=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_ENABLE_FLOAT=0 -DPPHP_INT64=1 $(CFLAGS) \
		$(RUNTIME_COMMON_SOURCES) $(COMPILER_SOURCES) compiler/codegen.c \
		tools/disasm.c shell/p2sh.c shell/p2sh_device.c ports/host/main.c -o $@

$(NO_FLOAT_INT64_PBC_BINARY): FORCE $(RUNTIME_COMMON_SOURCES) tools/disasm.c ports/host/main.c
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_ENABLE_FLOAT=%,$(PBC_CPPFLAGS)) \
		-DPPHP_ENABLE_FLOAT=0 -DPPHP_INT64=1 $(CFLAGS) \
		$(RUNTIME_COMMON_SOURCES) tools/disasm.c ports/host/main.c -o $@

$(FLOAT_INT64_BINARY): FORCE $(RUNTIME_COMMON_SOURCES) src/float_format.c $(COMPILER_SOURCES) compiler/codegen.c tools/disasm.c shell/p2sh.c shell/p2sh_device.c ports/host/main.c
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_ENABLE_FLOAT=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_ENABLE_FLOAT=1 -DPPHP_INT64=1 -DPPHP_USE_DOUBLE=1 $(CFLAGS) \
		$(RUNTIME_COMMON_SOURCES) src/float_format.c $(COMPILER_SOURCES) \
		compiler/codegen.c tools/disasm.c shell/p2sh.c shell/p2sh_device.c \
		ports/host/main.c $(LDFLAGS) -lm -o $@

$(FLOAT_REFERENCE_BINARY): FORCE $(RUNTIME_COMMON_SOURCES) src/float_format.c $(COMPILER_SOURCES) compiler/codegen.c tools/disasm.c shell/p2sh.c shell/p2sh_device.c ports/host/main.c
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_ENABLE_FLOAT=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_ENABLE_FLOAT=1 $(CFLAGS) $(RUNTIME_COMMON_SOURCES) \
		src/float_format.c $(COMPILER_SOURCES) compiler/codegen.c tools/disasm.c \
		shell/p2sh.c shell/p2sh_device.c ports/host/main.c $(LDFLAGS) -lm -o $@

$(RP2040_HOST_BINARY): $(HOST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -DPPHP_INT64=0 -DPPHP_USE_DOUBLE=0 \
		-DPPHP_DEVICE_FLOAT_FORMAT=1 $(CFLAGS) \
		$(HOST_SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

$(RP2040_UBSAN_BINARY): FORCE $(RUNTIME_COMMON_SOURCES) src/float_format.c $(COMPILER_SOURCES) compiler/codegen.c tools/disasm.c shell/p2sh.c shell/p2sh_device.c ports/host/main.c
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_ENABLE_FLOAT=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_ENABLE_FLOAT=1 -DPPHP_INT64=0 -DPPHP_USE_DOUBLE=0 \
		-DPPHP_DEVICE_FLOAT_FORMAT=1 $(CFLAGS) -O1 -g \
		-fno-omit-frame-pointer -fsanitize=undefined \
		$(RUNTIME_COMMON_SOURCES) src/float_format.c $(COMPILER_SOURCES) \
		compiler/codegen.c tools/disasm.c shell/p2sh.c shell/p2sh_device.c \
		ports/host/main.c -fsanitize=undefined -lm -o $@

$(FLOAT_FORMAT_TEST_BINARY): FORCE src/float_format.c tests/unit/test_float_format.c
	@mkdir -p $(@D)
	$(CC) $(BASE_CPPFLAGS) -DPPHP_INT64=0 -DPPHP_USE_DOUBLE=0 \
		-DPPHP_DEVICE_FLOAT_FORMAT=1 $(CFLAGS) src/float_format.c \
		tests/unit/test_float_format.c $(LDFLAGS) $(LDLIBS) -o $@

$(WARNINGS_ON_HOST_BINARY): FORCE $(COMPILER_HOST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_WARNINGS=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_WARNINGS=1 $(CFLAGS) $(COMPILER_HOST_SOURCES) \
		$(LDFLAGS) $(LDLIBS) -o $@

$(WARNINGS_OFF_HOST_BINARY): FORCE $(COMPILER_HOST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_WARNINGS=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_WARNINGS=0 $(CFLAGS) $(COMPILER_HOST_SOURCES) \
		$(LDFLAGS) $(LDLIBS) -o $@

$(WARNINGS_ON_PBC_BINARY): FORCE $(PBC_HOST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_WARNINGS=%,$(PBC_CPPFLAGS)) \
		-DPPHP_WARNINGS=1 $(CFLAGS) $(PBC_HOST_SOURCES) \
		$(LDFLAGS) $(LDLIBS) -o $@

$(WARNINGS_OFF_PBC_BINARY): FORCE $(PBC_HOST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_WARNINGS=%,$(PBC_CPPFLAGS)) \
		-DPPHP_WARNINGS=0 $(CFLAGS) $(PBC_HOST_SOURCES) \
		$(LDFLAGS) $(LDLIBS) -o $@

$(WARNINGS_NO_FLOAT_BINARY): FORCE $(RUNTIME_COMMON_SOURCES) $(COMPILER_SOURCES) compiler/codegen.c tools/disasm.c shell/p2sh.c shell/p2sh_device.c ports/host/main.c
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_ENABLE_FLOAT=% -DPPHP_WARNINGS=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_ENABLE_FLOAT=0 -DPPHP_WARNINGS=1 $(CFLAGS) \
		$(RUNTIME_COMMON_SOURCES) $(COMPILER_SOURCES) compiler/codegen.c \
		tools/disasm.c shell/p2sh.c shell/p2sh_device.c ports/host/main.c \
		$(LDFLAGS) -o $@

$(WARNINGS_RP_BINARY): FORCE $(COMPILER_HOST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_WARNINGS=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_WARNINGS=1 -DPPHP_INT64=0 -DPPHP_USE_DOUBLE=0 \
		-DPPHP_DEVICE_FLOAT_FORMAT=1 $(CFLAGS) $(COMPILER_HOST_SOURCES) \
		$(LDFLAGS) $(LDLIBS) -o $@

$(WARNINGS_UBSAN_BINARY): FORCE $(COMPILER_HOST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_WARNINGS=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_WARNINGS=1 $(CFLAGS) -O1 -g -fno-omit-frame-pointer \
		-fsanitize=undefined $(COMPILER_HOST_SOURCES) \
		-fsanitize=undefined $(LDLIBS) -o $@

$(TYPECHECK_ON_HOST_BINARY): FORCE $(COMPILER_HOST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_TYPECHECK=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_TYPECHECK=1 $(CFLAGS) $(COMPILER_HOST_SOURCES) \
		$(LDFLAGS) $(LDLIBS) -o $@

$(TYPECHECK_OFF_HOST_BINARY): FORCE $(COMPILER_HOST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_TYPECHECK=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_TYPECHECK=0 $(CFLAGS) $(COMPILER_HOST_SOURCES) \
		$(LDFLAGS) $(LDLIBS) -o $@

$(TYPECHECK_ON_PBC_BINARY): FORCE $(PBC_HOST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_TYPECHECK=%,$(PBC_CPPFLAGS)) \
		-DPPHP_TYPECHECK=1 $(CFLAGS) $(PBC_HOST_SOURCES) \
		$(LDFLAGS) $(LDLIBS) -o $@

$(TYPECHECK_OFF_PBC_BINARY): FORCE $(PBC_HOST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_TYPECHECK=%,$(PBC_CPPFLAGS)) \
		-DPPHP_TYPECHECK=0 $(CFLAGS) $(PBC_HOST_SOURCES) \
		$(LDFLAGS) $(LDLIBS) -o $@

$(TYPECHECK_NO_FLOAT_BINARY): FORCE $(RUNTIME_COMMON_SOURCES) $(COMPILER_SOURCES) compiler/codegen.c tools/disasm.c shell/p2sh.c shell/p2sh_device.c ports/host/main.c
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_TYPECHECK=% -DPPHP_ENABLE_FLOAT=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_TYPECHECK=1 -DPPHP_ENABLE_FLOAT=0 $(CFLAGS) \
		$(RUNTIME_COMMON_SOURCES) $(COMPILER_SOURCES) compiler/codegen.c \
		tools/disasm.c shell/p2sh.c shell/p2sh_device.c ports/host/main.c \
		$(LDFLAGS) -o $@

$(TYPECHECK_NO_FLOAT_OFF_BINARY): FORCE $(RUNTIME_COMMON_SOURCES) $(COMPILER_SOURCES) compiler/codegen.c tools/disasm.c shell/p2sh.c shell/p2sh_device.c ports/host/main.c
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_TYPECHECK=% -DPPHP_ENABLE_FLOAT=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_TYPECHECK=0 -DPPHP_ENABLE_FLOAT=0 $(CFLAGS) \
		$(RUNTIME_COMMON_SOURCES) $(COMPILER_SOURCES) compiler/codegen.c \
		tools/disasm.c shell/p2sh.c shell/p2sh_device.c ports/host/main.c \
		$(LDFLAGS) -o $@

$(TYPECHECK_INT64_BINARY): FORCE $(COMPILER_HOST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_TYPECHECK=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_TYPECHECK=1 -DPPHP_INT64=1 $(CFLAGS) \
		$(COMPILER_HOST_SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

$(TYPECHECK_RP_BINARY): FORCE $(COMPILER_HOST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_TYPECHECK=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_TYPECHECK=1 -DPPHP_INT64=0 -DPPHP_USE_DOUBLE=0 \
		-DPPHP_DEVICE_FLOAT_FORMAT=1 $(CFLAGS) $(COMPILER_HOST_SOURCES) \
		$(LDFLAGS) $(LDLIBS) -o $@

$(TYPECHECK_UBSAN_BINARY): FORCE $(COMPILER_HOST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_TYPECHECK=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_TYPECHECK=1 $(CFLAGS) -O1 -g -fno-omit-frame-pointer \
		-fsanitize=undefined $(COMPILER_HOST_SOURCES) \
		-fsanitize=undefined $(LDLIBS) -o $@

$(TYPECHECK_ASAN_BINARY): FORCE $(COMPILER_HOST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_TYPECHECK=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_TYPECHECK=1 $(CFLAGS) -O1 -g -fno-omit-frame-pointer \
		-fsanitize=address,undefined $(COMPILER_HOST_SOURCES) \
		-fsanitize=address,undefined $(LDLIBS) -o $@

$(TYPECHECK_VM_TEST_BINARY): FORCE $(VM_TEST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_TYPECHECK=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_TYPECHECK=1 $(CFLAGS) $(VM_TEST_SOURCES) $(LDFLAGS) \
		$(LDLIBS) -o $@

$(TYPECHECK_VM_UBSAN_BINARY): FORCE $(VM_TEST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_TYPECHECK=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_TYPECHECK=1 $(CFLAGS) -O1 -g -fno-omit-frame-pointer \
		-fsanitize=undefined $(VM_TEST_SOURCES) -fsanitize=undefined \
		$(LDLIBS) -o $@

$(TYPECHECK_VM_ASAN_BINARY): FORCE $(VM_TEST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_TYPECHECK=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_TYPECHECK=1 $(CFLAGS) -O1 -g -fno-omit-frame-pointer \
		-fsanitize=address,undefined $(VM_TEST_SOURCES) \
		-fsanitize=address,undefined $(LDLIBS) -o $@

$(RC_DEBUG_ON_HOST_BINARY): FORCE $(COMPILER_HOST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_RC_DEBUG=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_RC_DEBUG=1 $(CFLAGS) $(COMPILER_HOST_SOURCES) \
		$(LDFLAGS) $(LDLIBS) -o $@

$(RC_DEBUG_OFF_HOST_BINARY): FORCE $(COMPILER_HOST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_RC_DEBUG=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_RC_DEBUG=0 $(CFLAGS) $(COMPILER_HOST_SOURCES) \
		$(LDFLAGS) $(LDLIBS) -o $@

$(RC_DEBUG_NO_FLOAT_BINARY): FORCE $(RUNTIME_COMMON_SOURCES) $(COMPILER_SOURCES) compiler/codegen.c tools/disasm.c shell/p2sh.c shell/p2sh_device.c ports/host/main.c
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_ENABLE_FLOAT=% -DPPHP_RC_DEBUG=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_ENABLE_FLOAT=0 -DPPHP_RC_DEBUG=1 $(CFLAGS) \
		$(RUNTIME_COMMON_SOURCES) $(COMPILER_SOURCES) compiler/codegen.c \
		tools/disasm.c shell/p2sh.c shell/p2sh_device.c ports/host/main.c \
		$(LDFLAGS) -o $@

$(RC_DEBUG_COMPILER_OFF_BINARY): FORCE $(PBC_HOST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_RC_DEBUG=%,$(PBC_CPPFLAGS)) \
		-DPPHP_RC_DEBUG=1 $(CFLAGS) $(PBC_HOST_SOURCES) \
		$(LDFLAGS) $(LDLIBS) -o $@

$(RC_DEBUG_TEST_BINARY): FORCE $(RC_DEBUG_TEST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_RC_DEBUG=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_RC_DEBUG=1 $(CFLAGS) $(RC_DEBUG_TEST_SOURCES) \
		$(LDFLAGS) $(LDLIBS) -o $@

$(RC_DEBUG_UBSAN_BINARY): FORCE $(RC_DEBUG_TEST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_RC_DEBUG=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_RC_DEBUG=1 $(CFLAGS) -O1 -g -fno-omit-frame-pointer \
		-fsanitize=undefined $(RC_DEBUG_TEST_SOURCES) \
		-fsanitize=undefined $(LDLIBS) -o $@

$(RC_DEBUG_ASAN_BINARY): FORCE $(RC_DEBUG_TEST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_RC_DEBUG=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_RC_DEBUG=1 $(CFLAGS) -O1 -g -fno-omit-frame-pointer \
		-fsanitize=address,undefined $(RC_DEBUG_TEST_SOURCES) \
		-fsanitize=address,undefined $(LDLIBS) -o $@

$(RC_DEBUG_TYPECHECK_BINARY): FORCE $(RC_DEBUG_TEST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(filter-out -DPPHP_RC_DEBUG=% -DPPHP_TYPECHECK=%,$(COMPILER_CPPFLAGS)) \
		-DPPHP_RC_DEBUG=1 -DPPHP_TYPECHECK=1 $(CFLAGS) \
		$(RC_DEBUG_TEST_SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_BINARY): FORCE $(TEST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TEST_SOURCES) $(LDFLAGS) -o $@

$(LEXER_TEST_BINARY): FORCE $(LEXER_TEST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LEXER_TEST_SOURCES) $(LDFLAGS) -o $@

$(PARSER_TEST_BINARY): FORCE $(PARSER_TEST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PARSER_TEST_SOURCES) $(LDFLAGS) -o $@

$(VM_TEST_BINARY): FORCE $(VM_TEST_SOURCES)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(VM_TEST_SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

ifeq ($(PPHP_ENABLE_FLOAT),1)
TEST_UNIT_DEPENDENCIES := $(TEST_BINARY) $(LEXER_TEST_BINARY) $(PARSER_TEST_BINARY) $(VM_TEST_BINARY) $(FLOAT_FORMAT_TEST_BINARY) $(HOST_BINARY)
TEST_DEPENDENCIES := test-unit test-compiler-off test-no-float test-float-format test-warnings test-typecheck test-rc-debug test-phpt
else
TEST_UNIT_DEPENDENCIES := $(TEST_BINARY) $(LEXER_TEST_BINARY) $(PARSER_TEST_BINARY) $(HOST_BINARY)
TEST_DEPENDENCIES := test-unit test-compiler-off test-no-float test-warnings test-typecheck test-rc-debug
endif

test-unit: $(TEST_UNIT_DEPENDENCIES)
	$(TEST_BINARY)
	$(LEXER_TEST_BINARY)
	$(PARSER_TEST_BINARY)
ifeq ($(PPHP_ENABLE_FLOAT),1)
	$(VM_TEST_BINARY)
	$(FLOAT_FORMAT_TEST_BINARY)
endif
	$(HOST_BINARY) --version
	sh tests/cli/smoke.sh $(HOST_BINARY)

test-compiler-off: $(HOST_BINARY) $(CONFIGURED_PBC_HOST_BINARY) $(COMPILER_OFF_TEST_BINARY) $(COMPILER_OFF_DEVICE_OBJECT)
	$(COMPILER_OFF_TEST_BINARY)
	sh tests/cli/compiler_off.sh $(HOST_BINARY) $(CONFIGURED_PBC_HOST_BINARY)

test-no-float: $(FLOAT_REFERENCE_BINARY) $(NO_FLOAT_HOST_BINARY) $(NO_FLOAT_PBC_HOST_BINARY) test-no-float-ubsan
	sh tests/cli/no_float.sh $(FLOAT_REFERENCE_BINARY) $(NO_FLOAT_HOST_BINARY) $(NO_FLOAT_PBC_HOST_BINARY)

test-no-float-ubsan: $(NO_FLOAT_HOST_BINARY) $(NO_FLOAT_UBSAN_BINARY) $(NO_FLOAT_INT64_BINARY) $(NO_FLOAT_INT64_PBC_BINARY)
	sh tests/cli/no_float_overflow.sh $(NO_FLOAT_HOST_BINARY)
	sh tests/cli/no_float_overflow.sh $(NO_FLOAT_UBSAN_BINARY)
	sh tests/cli/no_float_int64.sh $(NO_FLOAT_INT64_BINARY) $(NO_FLOAT_INT64_PBC_BINARY)

test-float-format: $(RP2040_HOST_BINARY) $(FLOAT_FORMAT_TEST_BINARY) test-rp-integer-boundary
	$(FLOAT_FORMAT_TEST_BINARY)
	sh tests/cli/float_format_device.sh $(RP2040_HOST_BINARY)

test-rp-integer-boundary: $(RP2040_UBSAN_BINARY) $(FLOAT_INT64_BINARY)
	sh tests/cli/integer_float_boundary.sh $(RP2040_UBSAN_BINARY) \
		$(FLOAT_INT64_BINARY)

test-warnings-config: FORCE tests/unit/test_warnings_config.c
	@mkdir -p build/host
	$(CC) -Iinclude -DPPHP_WARNINGS=1 $(CFLAGS) -c \
		tests/unit/test_warnings_config.c -o $(WARNINGS_CONFIG_ON_OBJECT)
	$(CC) -Iinclude -DPPHP_WARNINGS=0 $(CFLAGS) -c \
		tests/unit/test_warnings_config.c -o $(WARNINGS_CONFIG_OFF_OBJECT)
	@if $(CC) -Iinclude -DPPHP_WARNINGS=2 $(CFLAGS) -c \
		tests/unit/test_warnings_config.c \
		-o $(WARNINGS_CONFIG_INVALID_OBJECT) \
		>$(WARNINGS_CONFIG_INVALID_LOG) 2>&1; then \
		echo 'PPHP_WARNINGS=2 unexpectedly compiled' >&2; \
		exit 1; \
	fi
	@grep -q 'PPHP_WARNINGS must be 0 or 1' \
		$(WARNINGS_CONFIG_INVALID_LOG)

test-warnings: test-warnings-config $(WARNINGS_ON_HOST_BINARY) $(WARNINGS_OFF_HOST_BINARY) $(WARNINGS_ON_PBC_BINARY) $(WARNINGS_OFF_PBC_BINARY) $(WARNINGS_NO_FLOAT_BINARY) $(WARNINGS_RP_BINARY) $(WARNINGS_UBSAN_BINARY)
	sh tests/cli/warnings.sh $(WARNINGS_ON_HOST_BINARY) \
		$(WARNINGS_OFF_HOST_BINARY) $(WARNINGS_ON_PBC_BINARY) \
		$(WARNINGS_OFF_PBC_BINARY) $(WARNINGS_NO_FLOAT_BINARY) \
		$(WARNINGS_RP_BINARY) $(WARNINGS_UBSAN_BINARY)

test-typecheck-config: FORCE tests/unit/test_typecheck_config.c
	@mkdir -p build/host
	$(CC) -Iinclude -DPPHP_TYPECHECK=1 $(CFLAGS) -c \
		tests/unit/test_typecheck_config.c -o $(TYPECHECK_CONFIG_ON_OBJECT)
	$(CC) -Iinclude -DPPHP_TYPECHECK=0 $(CFLAGS) -c \
		tests/unit/test_typecheck_config.c -o $(TYPECHECK_CONFIG_OFF_OBJECT)
	@if $(CC) -Iinclude -DPPHP_TYPECHECK=2 $(CFLAGS) -c \
		tests/unit/test_typecheck_config.c \
		-o $(TYPECHECK_CONFIG_INVALID_OBJECT) \
		>$(TYPECHECK_CONFIG_INVALID_LOG) 2>&1; then \
		echo 'PPHP_TYPECHECK=2 unexpectedly compiled' >&2; \
		exit 1; \
	fi
	@grep -q 'PPHP_TYPECHECK must be 0 or 1' \
		$(TYPECHECK_CONFIG_INVALID_LOG)

test-typecheck: test-typecheck-config $(TYPECHECK_ON_HOST_BINARY) $(TYPECHECK_OFF_HOST_BINARY) $(TYPECHECK_ON_PBC_BINARY) $(TYPECHECK_OFF_PBC_BINARY) $(TYPECHECK_NO_FLOAT_BINARY) $(TYPECHECK_NO_FLOAT_OFF_BINARY) $(TYPECHECK_INT64_BINARY) $(TYPECHECK_RP_BINARY) $(TYPECHECK_UBSAN_BINARY) $(TYPECHECK_ASAN_BINARY) $(TYPECHECK_VM_TEST_BINARY) $(TYPECHECK_VM_UBSAN_BINARY) $(TYPECHECK_VM_ASAN_BINARY)
	sh tests/cli/typecheck.sh $(TYPECHECK_ON_HOST_BINARY) \
		$(TYPECHECK_OFF_HOST_BINARY) $(TYPECHECK_ON_PBC_BINARY) \
		$(TYPECHECK_OFF_PBC_BINARY) $(TYPECHECK_NO_FLOAT_BINARY) \
		$(TYPECHECK_INT64_BINARY) $(TYPECHECK_RP_BINARY) \
		$(TYPECHECK_UBSAN_BINARY) $(TYPECHECK_ASAN_BINARY) \
		$(TYPECHECK_NO_FLOAT_OFF_BINARY)
	$(TYPECHECK_VM_TEST_BINARY)
	$(TYPECHECK_VM_UBSAN_BINARY)
	ASAN_OPTIONS=detect_leaks=$(ASAN_LEAKS) $(TYPECHECK_VM_ASAN_BINARY)

test-rc-debug-config: FORCE tests/unit/test_rc_debug_config.c tests/unit/test_rc_debug_visitor_api.c
	@mkdir -p build/host
	$(CC) -Iinclude -DPPHP_RC_DEBUG=1 $(CFLAGS) -c \
		tests/unit/test_rc_debug_config.c -o $(RC_DEBUG_CONFIG_ON_OBJECT)
	$(CC) -Iinclude -DPPHP_RC_DEBUG=0 $(CFLAGS) -c \
		tests/unit/test_rc_debug_config.c -o $(RC_DEBUG_CONFIG_OFF_OBJECT)
	@if $(CC) -Iinclude -DPPHP_RC_DEBUG=2 $(CFLAGS) -c \
		tests/unit/test_rc_debug_config.c \
		-o $(RC_DEBUG_CONFIG_INVALID_OBJECT) \
		>$(RC_DEBUG_CONFIG_INVALID_LOG) 2>&1; then \
		echo 'PPHP_RC_DEBUG=2 unexpectedly compiled' >&2; \
		exit 1; \
	fi
	@grep -q 'PPHP_RC_DEBUG must be 0 or 1' \
		$(RC_DEBUG_CONFIG_INVALID_LOG)
	$(CC) -Iinclude -DPPHP_RC_DEBUG=1 $(CFLAGS) -c \
		tests/unit/test_rc_debug_visitor_api.c \
		-o $(RC_DEBUG_VISITOR_API_OBJECT)
	@if $(MAKE) -f Makefile PPHP_RC_DEBUG=2 -n host >/dev/null 2>&1; then \
		echo 'make PPHP_RC_DEBUG=2 unexpectedly succeeded' >&2; \
		exit 1; \
	fi

test-rc-debug: test-rc-debug-config $(RC_DEBUG_ON_HOST_BINARY) $(RC_DEBUG_OFF_HOST_BINARY) $(RC_DEBUG_NO_FLOAT_BINARY) $(RC_DEBUG_COMPILER_OFF_BINARY) $(RC_DEBUG_TEST_BINARY) $(RC_DEBUG_UBSAN_BINARY) $(RC_DEBUG_ASAN_BINARY) $(RC_DEBUG_TYPECHECK_BINARY)
	$(RC_DEBUG_TEST_BINARY)
	$(RC_DEBUG_UBSAN_BINARY)
	ASAN_OPTIONS=detect_leaks=$(ASAN_LEAKS) $(RC_DEBUG_ASAN_BINARY)
	$(RC_DEBUG_TYPECHECK_BINARY)
	sh tests/cli/rc_debug.sh $(RC_DEBUG_ON_HOST_BINARY) \
		$(RC_DEBUG_OFF_HOST_BINARY)

test-phpt: $(HOST_BINARY)
	sh tools/phpt_run.sh --binary $(HOST_BINARY) tests/phpt

test-target:
	@test -n "$(PORT)" || { echo "usage: make test-target PORT=/dev/ttyACM0"; exit 2; }
	sh tools/phpt_run.sh --target=serial --port "$(PORT)" tests/phpt

test: $(TEST_DEPENDENCIES)

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
	sh tools/difftest.sh --binary $(HOST_BINARY) tests/phpt

bench: host
	python3 tools/bench.py --binary $(HOST_BINARY)

size: host
	sh tools/sizecheck.sh $(HOST_BINARY)

clean:
	rm -rf build
