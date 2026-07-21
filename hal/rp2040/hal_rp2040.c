#include "pphp/hal.h"

#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/flash.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "hardware/sync.h"
#include "hardware/uart.h"
#include "hardware/watchdog.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#include "pico/stdlib.h"
#include "pico/unique_id.h"

#include <string.h>

#ifndef PPHP_FLASH_FS_SIZE
#define PPHP_FLASH_FS_SIZE (1024U * 1024U)
#endif

typedef struct hal_event {
    uint8_t type;
    uint8_t id;
    uint32_t argument;
} hal_event;

static volatile hal_event event_queue[16];
static volatile uint8_t event_read;
static volatile uint8_t event_write;
static uint32_t random_state;
static uint16_t pwm_wraps[30];

static i2c_inst_t *i2c_instance(uint8_t unit) {
    return unit == 0U ? i2c0 : (unit == 1U ? i2c1 : NULL);
}

static spi_inst_t *spi_instance(uint8_t unit) {
    return unit == 0U ? spi0 : (unit == 1U ? spi1 : NULL);
}

static uart_inst_t *uart_instance(uint8_t unit) {
    return unit == 0U ? uart0 : (unit == 1U ? uart1 : NULL);
}

static void gpio_irq_callback(uint gpio, uint32_t events) {
    uint8_t translated = 0U;
    if ((events & GPIO_IRQ_EDGE_RISE) != 0U) translated |= 1U;
    if ((events & GPIO_IRQ_EDGE_FALL) != 0U) translated |= 2U;
    if ((events & GPIO_IRQ_LEVEL_HIGH) != 0U) translated |= 4U;
    if ((events & GPIO_IRQ_LEVEL_LOW) != 0U) translated |= 8U;
    (void)hal_event_push(1U, (uint8_t)gpio, translated);
}

int hal_init(void) {
    stdio_init_all();
    adc_init();
    random_state = (uint32_t)time_us_64() ^ UINT32_C(0xa5c31e27);
    memset(pwm_wraps, 0xff, sizeof(pwm_wraps));
    return PPHP_HAL_OK;
}

int hal_gpio_init(uint8_t pin, uint8_t mode, uint8_t pull) {
    if (pin >= 30U) return PPHP_HAL_INVALID;
    gpio_init(pin);
    gpio_set_dir(pin, mode == 2U ? GPIO_OUT : GPIO_IN);
    gpio_set_pulls(pin, (pull & 8U) != 0U, (pull & 16U) != 0U);
    return PPHP_HAL_OK;
}

int hal_gpio_write(uint8_t pin, uint8_t level) {
    if (pin >= 30U) return PPHP_HAL_INVALID;
    gpio_put(pin, level != 0U);
    return PPHP_HAL_OK;
}

int hal_gpio_read(uint8_t pin) {
    return pin >= 30U ? PPHP_HAL_INVALID : (gpio_get(pin) ? 1 : 0);
}

int hal_gpio_irq_enable(uint8_t pin, uint8_t edges) {
    uint32_t mask = 0U;
    if (pin >= 30U) return PPHP_HAL_INVALID;
    if ((edges & 1U) != 0U) mask |= GPIO_IRQ_EDGE_RISE;
    if ((edges & 2U) != 0U) mask |= GPIO_IRQ_EDGE_FALL;
    if ((edges & 4U) != 0U) mask |= GPIO_IRQ_LEVEL_HIGH;
    if ((edges & 8U) != 0U) mask |= GPIO_IRQ_LEVEL_LOW;
    gpio_set_irq_enabled_with_callback(pin, mask, true, gpio_irq_callback);
    return PPHP_HAL_OK;
}

int hal_adc_init(uint8_t pin) {
    if (pin < 26U || pin > 29U) return PPHP_HAL_INVALID;
    adc_gpio_init(pin);
    return PPHP_HAL_OK;
}

int hal_adc_read_u16(uint8_t pin, uint16_t *out) {
    uint16_t raw;
    if (pin < 26U || pin > 29U || out == NULL) return PPHP_HAL_INVALID;
    adc_select_input((uint)(pin - 26U));
    raw = adc_read();
    *out = (uint16_t)((raw << 4U) | (raw >> 8U));
    return PPHP_HAL_OK;
}

int hal_pwm_init(uint8_t pin, uint32_t hz) {
    uint slice;
    uint32_t clock;
    uint32_t divider16;
    uint32_t wrap;
    if (pin >= 30U || hz == 0U) return PPHP_HAL_INVALID;
    gpio_set_function(pin, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(pin);
    clock = clock_get_hz(clk_sys);
    divider16 = (clock + hz * UINT32_C(65535) - 1U) /
                (hz * UINT32_C(65535));
    if (divider16 < 16U) divider16 = 16U;
    if (divider16 > 4095U) divider16 = 4095U;
    wrap = (uint32_t)(((uint64_t)clock * 16U) /
                      ((uint64_t)hz * divider16));
    if (wrap == 0U) wrap = 1U;
    if (wrap > UINT16_MAX) wrap = UINT16_MAX;
    pwm_set_clkdiv_int_frac(slice, (uint8_t)(divider16 >> 4U),
                            (uint8_t)(divider16 & 15U));
    pwm_set_wrap(slice, (uint16_t)(wrap - 1U));
    pwm_set_enabled(slice, true);
    pwm_wraps[pin] = (uint16_t)(wrap - 1U);
    return PPHP_HAL_OK;
}

int hal_pwm_set_duty_u16(uint8_t pin, uint16_t duty) {
    uint32_t level;
    if (pin >= 30U) return PPHP_HAL_INVALID;
    level = ((uint32_t)pwm_wraps[pin] + 1U) * duty / UINT32_C(65535);
    pwm_set_gpio_level(pin, (uint16_t)level);
    return PPHP_HAL_OK;
}

int hal_pwm_set_period_us(uint8_t pin, uint32_t us, uint32_t pulse_us) {
    uint32_t hz;
    uint32_t level;
    if (pin >= 30U || us == 0U || pulse_us > us) return PPHP_HAL_INVALID;
    hz = UINT32_C(1000000) / us;
    if (hz == 0U || hal_pwm_init(pin, hz) != PPHP_HAL_OK) {
        return PPHP_HAL_INVALID;
    }
    level = ((uint32_t)pwm_wraps[pin] + 1U) * pulse_us / us;
    pwm_set_gpio_level(pin, (uint16_t)level);
    return PPHP_HAL_OK;
}

int hal_i2c_init(uint8_t unit, uint8_t sda, uint8_t scl, uint32_t hz) {
    i2c_inst_t *instance = i2c_instance(unit);
    if (instance == NULL || sda >= 30U || scl >= 30U || hz == 0U) {
        return PPHP_HAL_INVALID;
    }
    (void)i2c_init(instance, hz);
    gpio_set_function(sda, GPIO_FUNC_I2C);
    gpio_set_function(scl, GPIO_FUNC_I2C);
    gpio_pull_up(sda);
    gpio_pull_up(scl);
    return PPHP_HAL_OK;
}

int hal_i2c_write(uint8_t unit, uint8_t addr, const uint8_t *bytes,
                  size_t length, int stop) {
    i2c_inst_t *instance = i2c_instance(unit);
    if (instance == NULL || (length != 0U && bytes == NULL)) {
        return PPHP_HAL_INVALID;
    }
    return i2c_write_blocking(instance, addr, bytes, length, stop == 0);
}

int hal_i2c_read(uint8_t unit, uint8_t addr, uint8_t *bytes, size_t length) {
    i2c_inst_t *instance = i2c_instance(unit);
    if (instance == NULL || (length != 0U && bytes == NULL)) {
        return PPHP_HAL_INVALID;
    }
    return i2c_read_blocking(instance, addr, bytes, length, false);
}

int hal_spi_init(uint8_t unit, uint8_t sck, uint8_t mosi, uint8_t miso,
                 uint32_t hz, uint8_t mode, uint8_t bits) {
    spi_inst_t *instance = spi_instance(unit);
    if (instance == NULL || sck >= 30U || mosi >= 30U || miso >= 30U ||
        hz == 0U || mode > 3U || (bits != 8U && bits != 16U)) {
        return PPHP_HAL_INVALID;
    }
    (void)spi_init(instance, hz);
    spi_set_format(instance, bits,
                   (mode & 2U) != 0U ? SPI_CPOL_1 : SPI_CPOL_0,
                   (mode & 1U) != 0U ? SPI_CPHA_1 : SPI_CPHA_0,
                   SPI_MSB_FIRST);
    gpio_set_function(sck, GPIO_FUNC_SPI);
    gpio_set_function(mosi, GPIO_FUNC_SPI);
    gpio_set_function(miso, GPIO_FUNC_SPI);
    return PPHP_HAL_OK;
}

int hal_spi_transfer(uint8_t unit, const uint8_t *tx, uint8_t *rx,
                     size_t length) {
    spi_inst_t *instance = spi_instance(unit);
    if (instance == NULL || (length != 0U && tx == NULL)) {
        return PPHP_HAL_INVALID;
    }
    if (rx == NULL) return spi_write_blocking(instance, tx, length);
    return spi_write_read_blocking(instance, tx, rx, length);
}

int hal_uart_init(uint8_t unit, uint8_t tx, uint8_t rx, uint32_t baud,
                  uint8_t data, uint8_t parity, uint8_t stop) {
    uart_inst_t *instance = uart_instance(unit);
    uart_parity_t parity_mode = parity == 0U
                                    ? UART_PARITY_NONE
                                    : (parity == 1U ? UART_PARITY_ODD
                                                    : UART_PARITY_EVEN);
    if (instance == NULL || tx >= 30U || rx >= 30U || baud == 0U ||
        data < 5U || data > 8U || stop < 1U || stop > 2U || parity > 2U) {
        return PPHP_HAL_INVALID;
    }
    (void)uart_init(instance, baud);
    gpio_set_function(tx, GPIO_FUNC_UART);
    gpio_set_function(rx, GPIO_FUNC_UART);
    uart_set_format(instance, data, stop, parity_mode);
    uart_set_fifo_enabled(instance, true);
    return PPHP_HAL_OK;
}

int hal_uart_write(uint8_t unit, const uint8_t *bytes, size_t length) {
    uart_inst_t *instance = uart_instance(unit);
    if (instance == NULL || (length != 0U && bytes == NULL)) {
        return PPHP_HAL_INVALID;
    }
    uart_write_blocking(instance, bytes, length);
    return (int)length;
}

int hal_uart_read(uint8_t unit, uint8_t *bytes, size_t length) {
    uart_inst_t *instance = uart_instance(unit);
    size_t count = 0U;
    if (instance == NULL || (length != 0U && bytes == NULL)) {
        return PPHP_HAL_INVALID;
    }
    while (count < length && uart_is_readable(instance)) {
        bytes[count++] = (uint8_t)uart_getc(instance);
    }
    return (int)count;
}

uint64_t hal_time_us(void) { return time_us_64(); }
void hal_sleep_ms(uint32_t milliseconds) { sleep_ms(milliseconds); }
void hal_deep_sleep_ms(uint32_t milliseconds) { sleep_ms(milliseconds); }

void hal_reset(void) {
    watchdog_reboot(0U, 0U, 0U);
    for (;;) tight_loop_contents();
}

uint32_t hal_cpu_freq(void) { return clock_get_hz(clk_sys); }

int hal_unique_id(uint8_t *buffer, size_t length) {
    pico_unique_board_id_t id;
    if (buffer == NULL) return PPHP_HAL_INVALID;
    pico_get_unique_board_id(&id);
    if (length > sizeof(id.id)) length = sizeof(id.id);
    memcpy(buffer, id.id, length);
    return PPHP_HAL_OK;
}

uint32_t hal_random(void) {
    random_state ^= random_state << 13U;
    random_state ^= random_state >> 17U;
    random_state ^= random_state << 5U;
    random_state ^= (uint32_t)time_us_64();
    return random_state;
}

void hal_console_write(const char *bytes, size_t length) {
    size_t i;
    if (bytes == NULL) return;
    for (i = 0U; i < length; i++) putchar_raw((int)(uint8_t)bytes[i]);
}

int hal_console_read(uint8_t *bytes, size_t length) {
    size_t count = 0U;
    if (bytes == NULL) return PPHP_HAL_INVALID;
    while (count < length) {
        int value = getchar_timeout_us(0U);
        if (value == PICO_ERROR_TIMEOUT) break;
        bytes[count++] = (uint8_t)value;
    }
    return (int)count;
}

int hal_interrupt_requested(void) {
    int value = getchar_timeout_us(0U);
    return value == 3;
}

int __no_inline_not_in_flash_func(hal_bootsel_pressed)(void) {
    const uint32_t pin = 1U;
    uint32_t interrupts = save_and_disable_interrupts();
    uint32_t delay;
    int pressed;
    hw_write_masked(&ioqspi_hw->io[pin].ctrl,
                    GPIO_OVERRIDE_LOW <<
                        IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
    for (delay = 0U; delay < 1000U; delay++) {
        __asm volatile ("nop");
    }
    pressed = (sio_hw->gpio_hi_in & (1UL << pin)) == 0U;
    hw_write_masked(&ioqspi_hw->io[pin].ctrl,
                    GPIO_OVERRIDE_NORMAL <<
                        IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
    restore_interrupts(interrupts);
    return pressed;
}

static uint32_t flash_offset(uint32_t offset) {
    return (uint32_t)PICO_FLASH_SIZE_BYTES - PPHP_FLASH_FS_SIZE + offset;
}

int hal_flash_read(uint32_t offset, void *buffer, size_t length) {
    if (buffer == NULL || offset > PPHP_FLASH_FS_SIZE ||
        length > PPHP_FLASH_FS_SIZE - offset) return PPHP_HAL_INVALID;
    memcpy(buffer, (const void *)(XIP_BASE + flash_offset(offset)), length);
    return PPHP_HAL_OK;
}

int hal_flash_prog(uint32_t offset, const void *buffer, size_t length) {
    uint32_t interrupts;
    if (buffer == NULL || offset > PPHP_FLASH_FS_SIZE ||
        length > PPHP_FLASH_FS_SIZE - offset ||
        (flash_offset(offset) & (FLASH_PAGE_SIZE - 1U)) != 0U ||
        length % FLASH_PAGE_SIZE != 0U) return PPHP_HAL_INVALID;
    interrupts = save_and_disable_interrupts();
    flash_range_program(flash_offset(offset), buffer, length);
    restore_interrupts(interrupts);
    return PPHP_HAL_OK;
}

int hal_flash_erase(uint32_t block) {
    uint32_t offset = block * FLASH_SECTOR_SIZE;
    uint32_t interrupts;
    if (offset >= PPHP_FLASH_FS_SIZE) return PPHP_HAL_INVALID;
    interrupts = save_and_disable_interrupts();
    flash_range_erase(flash_offset(offset), FLASH_SECTOR_SIZE);
    restore_interrupts(interrupts);
    return PPHP_HAL_OK;
}

int hal_event_push(uint8_t type, uint8_t id, uint32_t argument) {
    uint8_t next = (uint8_t)((event_write + 1U) % 16U);
    if (next == event_read) return PPHP_HAL_ERROR;
    event_queue[event_write].type = type;
    event_queue[event_write].id = id;
    event_queue[event_write].argument = argument;
    __dmb();
    event_write = next;
    __sev();
    return PPHP_HAL_OK;
}

int hal_event_pop(uint8_t *type, uint8_t *id, uint32_t *argument) {
    uint32_t interrupts = save_and_disable_interrupts();
    uint8_t index = event_read;
    if (index == event_write) {
        restore_interrupts(interrupts);
        return 0;
    }
    if (type != NULL) *type = event_queue[index].type;
    if (id != NULL) *id = event_queue[index].id;
    if (argument != NULL) *argument = event_queue[index].argument;
    event_read = (uint8_t)((index + 1U) % 16U);
    restore_interrupts(interrupts);
    return 1;
}

int hal_wdt_enable(uint32_t milliseconds) {
    if (milliseconds == 0U) return PPHP_HAL_INVALID;
    watchdog_enable(milliseconds, true);
    return PPHP_HAL_OK;
}

void hal_wdt_feed(void) { watchdog_update(); }
