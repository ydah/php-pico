# RP2040 hardware smoke test

Build with `PICO_SDK_PATH` set, copy `build/rp2040/php-pico.uf2` while the
board is in BOOTSEL mode, then connect to its USB CDC serial port at 115200.
UART0 remains enabled as a fallback console.

Run the examples in this order:

1. `examples/blink.php` with the board LED on GPIO 25.
2. `examples/adc_read.php` with a 0–3.3 V source on GPIO 26.
3. `examples/i2c_scan.php` with SDA=GPIO 4 and SCL=GPIO 5.
4. `examples/pwm_servo.php` with a servo signal on GPIO 15.
5. `examples/uart_echo.php` with UART1 TX/RX on GPIO 4/5.

For the endurance check, run blink plus one I2C read per second for 24 hours.
Record `free` once per minute and confirm that used memory remains stable.
GPIO interrupt handlers must only enqueue events; PHP callbacks are dispatched
by the VM after the next 1 ms tick at an opcode boundary.

## Recovery and filesystem checks

1. Upload an infinite-loop script as `/home/app.php`, reset, and press Ctrl-C.
   Confirm that execution stops and `pico$` returns.
2. Reset while holding BOOTSEL. Confirm that neither `boot.php` nor the app is
   run and the shell appears.
3. Compile `app.php` to `app.pbc`, reset, and confirm the PBC takes precedence.
4. During a repeated 4 KiB upload, remove power at a different point for each
   of 100 cycles. After every reconnect, mount the filesystem, run `ls`, read
   previously committed files, remove the interrupted file, and upload it
   again. No previously committed file may change or disappear.
5. Run `make test-target PORT=/dev/ttyACM0` over USB CDC. Repeat essential
   console checks over UART0 to validate the fallback path.
