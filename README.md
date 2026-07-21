# php-pico

`php-pico` is a small, dependency-free PHP 8.3 subset runtime written in C99.
It targets both POSIX hosts and the Raspberry Pi Pico (RP2040).

## Build and test

```sh
make host
make test
make test-asan
```

The host executable is written to `build/host/php-pico`.
