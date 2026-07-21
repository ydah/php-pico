# RP2040 quick start

## 1. Build and flash

Use Pico SDK 2.3.0 with its submodules and an ARM embedded toolchain that
includes newlib.

```sh
git submodule update --init
export PICO_SDK_PATH=/path/to/pico-sdk
make rp2040
```

Hold BOOTSEL, connect the Pico, and copy
`build/rp2040/php-pico-1.0.0.uf2` to the mounted RPI-RP2 volume. Open its USB
CDC serial device at 115200 baud. After the recovery window, `pico$` appears.

Useful P2Sh commands include `ls`, `cd`, `pwd`, `cat`, `mkdir`, `rm -r`, `mv`,
`cp`, `free`, `version`, `php`, `phpc`, `repl`, and `reboot`. Backspace, cursor
keys, and the last eight commands are supported.

## 2. Blink

The Pico board LED is GPIO 25. Upload the provided script:

```sh
python3 -m pip install pyserial
python3 tools/p2upload.py /dev/ttyACM0 examples/blink.php /home/blink.php
```

At P2Sh, run `php /home/blink.php`. Press Ctrl-C to interrupt it. To make it
the automatic application, upload it as `/home/app.php` and run `reboot`.

Hold BOOTSEL during reset, or press Ctrl-C during the three-second startup
window, whenever you need to bypass a broken automatic application.

## 3. Read a sensor

Connect an I2C device to GPIO 4 (SDA), GPIO 5 (SCL), 3V3, and GND. Upload and
run the scanner:

```sh
python3 tools/p2upload.py /dev/ttyACM0 examples/i2c_scan.php /home/scan.php
```

```text
pico$ php /home/scan.php
```

For analog input, connect a 0–3.3 V source to GPIO 26 and run
`examples/adc_read.php`. Do not exceed the RP2040 ADC voltage range.

## 4. Compile on the board

Source can be compiled into target-compatible bytecode without a host rebuild:

```text
pico$ phpc /home/app.php /home/app.pbc
pico$ php /home/app.pbc
```

At the next reset, `app.pbc` takes precedence over `app.php`. Remove the PBC to
return to source startup. Place configuration code in `/home/boot.php`; it runs
before the application.

For host-side compilation, build the RP2040-compatible compiler and upload its
output:

```sh
make host-rp2040
build/host/php-pico-rp2040 -c examples/blink.php -o build/host/blink.pbc
python3 tools/p2upload.py /dev/ttyACM0 build/host/blink.pbc /home/app.pbc
```

## 5. REPL and test runner

Run `repl` for persistent interactive definitions; type `exit` to return to
P2Sh. Lines beginning with `$` or `<?` at the shell are also evaluated as PHP.

With pyserial installed, the PHPT-mini suite can be uploaded and run case by
case on a connected board:

```sh
make test-target PORT=/dev/ttyACM0
```

Proceed through `docs/HARDWARE_TEST.md` before treating a build as a release.
