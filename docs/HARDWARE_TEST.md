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
