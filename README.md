# php-pico

`php-pico` is a compact PHP 8.3 subset runtime written in C99. It runs as a
POSIX command-line program and as RP2040 firmware with GPIO/I2C/SPI/UART gems,
littlefs storage, automatic application startup, and the P2Sh serial shell.

## Host build

```sh
make host
make test
make test-asan
make test-diff
make bench
make size
```

The executable is `build/host/php-pico`:

```sh
build/host/php-pico app.php
build/host/php-pico -r 'echo 6 * 7, PHP_EOL;'
build/host/php-pico -c app.php -o app.pbc
build/host/php-pico -d app.pbc
build/host/php-pico --shell
```

Set `PPHP_TRACE=1` when running the host CLI to print each instruction, program
counter, and stack depth to standard error.

For reference-count diagnostics, build with `PPHP_RC_DEBUG=1` (or
`-DPPHP_RC_DEBUG=ON` in the RP2040 CMake build) and run `rccheck` in P2Sh. The
command scans all reference-counted heap values and reports either `OK` or the
first actual/expected mismatch. This option is disabled by default; its heap
markers, object visitor field, checker API, and command are compiled out when
set to `0`.

Native object data that retains `pvalue` instances must register a
`pphp_native_rc_visit_fn` with `pphp_obj_set_rc_visitor()` in debug builds. The
visitor enumerates every owned value exactly once and must not retain, release,
or mutate values while the checker is running. Opaque native data with no
owned `pvalue` needs no visitor.

The normal host build uses double-precision floats, while RP2040 uses
single-precision floats. To produce bytecode whose header and constants are
target-compatible, use the dedicated cross-configuration compiler:

```sh
make host-rp2040
build/host/php-pico-rp2040 -c app.php -o app.pbc
```

## RP2040 firmware

Install the ARM embedded toolchain, newlib, CMake, and Raspberry Pi Pico SDK
2.3.0, including its submodules. Then build:

```sh
git submodule update --init
export PICO_SDK_PATH=/path/to/pico-sdk
make rp2040
```

The build emits `build/rp2040/php-pico.uf2` and
`build/rp2040/php-pico-1.0.0.uf2`. Hold BOOTSEL while connecting the Pico and
copy the UF2 to its mass-storage volume. P2Sh is available over USB CDC at
115200 baud; UART0 is the fallback.

Upload an application without terminal quoting or line-length limitations:

```sh
python3 -m pip install pyserial
python3 tools/p2upload.py /dev/ttyACM0 examples/blink.php /home/app.php
```

After reset, `/home/boot.php` runs first, followed by `/home/app.pbc` or
`/home/app.php`. Hold BOOTSEL or send Ctrl-C during the three-second recovery
window to skip automatic startup. Ctrl-C also interrupts a running VM at its
next 1 ms safe point.

See [`docs/TUTORIAL.md`](docs/TUTORIAL.md) for the end-to-end hardware guide,
[`docs/PORTING.md`](docs/PORTING.md) for HAL ports, and
[`docs/HARDWARE_TEST.md`](docs/HARDWARE_TEST.md) for release validation.

## Compatibility and releases

php-pico 1.0.0 reads and writes PPBC format version 2. A PBC is rejected when
its integer-width, floating-point-width, line-info, or runtime-typecheck flags
differ from the running build; recompile source for that target rather than
copying an incompatible image.

Declared parameter, return, and property types are parsed in every build.
Set `PPHP_TYPECHECK=1` (or `-DPPHP_TYPECHECK=ON` for the RP2040 CMake build) to
enforce them with catchable `TypeError` exceptions. The default is `0`, which
keeps declarations as compatibility syntax without adding runtime type
metadata. Type checking is exact and is unaffected by `strict_types`.

littlefs is pinned as a submodule at v2.11.3. The runtime itself does not call
the system allocator: host and device allocations go through the configured
php-pico pool.
