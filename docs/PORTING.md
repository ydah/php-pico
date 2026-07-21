# Porting php-pico

This guide describes the boundary required to run php-pico on a new board. The
RP2040 and POSIX implementations in `hal/` are the reference ports.

## Build contract

- Compile as C99 with `-Wall -Wextra -Werror`; VLA and `alloca` are forbidden.
- Define the options in `pphp_config.h` before including public headers.
- Reserve an aligned memory region and call `pphp_pool_init`, then `pphp_open`.
- Call `pphp_tick(state)` from a 1 ms timer. It only records a tick; callbacks
  are dispatched later by the VM, never in the ISR.
- Route program output with `pphp_set_output`.
- Do not call libc `malloc` from runtime or HAL code.
- Define `PPHP_TRACE=1` for opcode traces on the board console while debugging;
  leave it disabled in release builds.

## HAL functions

All declarations are in `include/pphp/hal.h`. Return `PPHP_HAL_OK`,
`PPHP_HAL_ERROR`, `PPHP_HAL_UNSUPPORTED`, or `PPHP_HAL_INVALID` unless the
function documents a byte count or value.

| Area | Functions | Contract |
|---|---|---|
| Lifecycle | `hal_init` | Initialize clocks, console, and peripheral state. |
| GPIO | `hal_gpio_init`, `hal_gpio_write`, `hal_gpio_read`, `hal_gpio_irq_enable` | IRQ handlers only call `hal_event_push`. |
| ADC | `hal_adc_init`, `hal_adc_read_u16` | Normalize a conversion to 0…65535. |
| PWM | `hal_pwm_init`, `hal_pwm_set_duty_u16`, `hal_pwm_set_period_us` | Preserve frequency/period when changing duty. |
| I2C | `hal_i2c_init`, `hal_i2c_write`, `hal_i2c_read` | Return transferred byte count, or a negative HAL error. |
| SPI | `hal_spi_init`, `hal_spi_transfer` | Full-duplex transfer; either buffer may be absent if the port supports it. |
| UART | `hal_uart_init`, `hal_uart_write`, `hal_uart_read` | Nonblocking reads are preferred for shell responsiveness. |
| Time | `hal_time_us`, `hal_sleep_ms`, `hal_deep_sleep_ms` | `hal_time_us` must be monotonic across its 64-bit range. |
| Machine | `hal_reset`, `hal_cpu_freq`, `hal_unique_id`, `hal_random` | Random output seeds the VM PRNG; use hardware entropy when available. |
| Console | `hal_console_write`, `hal_console_read` | Reads are nonblocking. USB CDC may be paired with a UART fallback. |
| Recovery | `hal_interrupt_requested`, `hal_bootsel_pressed` | Detect Ctrl-C while a task runs and the physical boot override at reset. |
| Flash | `hal_flash_read`, `hal_flash_prog`, `hal_flash_erase` | Offsets are relative to the reserved FS region. Honor native alignment. |
| Events | `hal_event_push`, `hal_event_pop` | Use a bounded, IRQ-safe queue. The VM is the only consumer. |
| Watchdog | `hal_wdt_enable`, `hal_wdt_feed` | Clamp unsupported timeout ranges or return `PPHP_HAL_INVALID`. |

## Filesystem

Implement the API in `include/pphp/fs.h`. A mount failure may format only the
dedicated filesystem region, never the firmware region. The reference littlefs
configuration uses 4 KiB erase blocks, 256-byte program/cache units, fixed
mount buffers, and a fixed 256-byte buffer per open file. `/home` maps to the
writable root. `/lib` is reserved for read-only PBC modules supplied by a port.

On flash-executing MCUs, erase/program routines must run from RAM and exclude
interrupt handlers that could fetch code from flash. Ensure the linked firmware
ends before the reserved filesystem offset.

## PPBC images and XIP

`pphp_exec_pbc` borrows its input image for the complete call. Keep the image
mapped and immutable until execution returns; the VM fetches opcodes directly
from that memory. Filesystem loaders must likewise retain their backing buffer
until the loaded module is destroyed. Never erase or program flash containing
an active PPBC image.

## Startup and recovery

The board port must perform this sequence:

1. initialize HAL, allocator, VM, pgems, and the 1 ms timer;
2. mount the filesystem, formatting it on first use;
3. provide a BOOTSEL/Ctrl-C recovery interval;
4. run `/home/boot.php`, then `app.pbc` before `app.php`;
5. enter P2Sh after completion, failure, exit, or interruption.

An uncaught exception must stop only the current PHP task. A bad or looping
application must never prevent the user from returning to the shell.

## Validation checklist

Run host unit, PHPT, differential, and sanitizer checks first. On the target,
exercise every HAL function, run `make test-target PORT=...`, confirm the five
hardware examples, interrupt an infinite loop with Ctrl-C, boot with BOOTSEL
held, and perform the power-loss test in `HARDWARE_TEST.md`. Record ELF size
with `tools/sizecheck.sh`.
