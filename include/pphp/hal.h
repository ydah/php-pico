#ifndef PPHP_HAL_H
#define PPHP_HAL_H

#include <stddef.h>
#include <stdint.h>

enum {
    PPHP_HAL_OK = 0,
    PPHP_HAL_ERROR = -1,
    PPHP_HAL_UNSUPPORTED = -2,
    PPHP_HAL_INVALID = -3
};

int hal_init(void);

int hal_gpio_init(uint8_t pin, uint8_t mode, uint8_t pull);
int hal_gpio_write(uint8_t pin, uint8_t level);
int hal_gpio_read(uint8_t pin);
int hal_gpio_irq_enable(uint8_t pin, uint8_t edges);

int hal_adc_init(uint8_t pin);
int hal_adc_read_u16(uint8_t pin, uint16_t *out);
int hal_pwm_init(uint8_t pin, uint32_t hz);
int hal_pwm_set_duty_u16(uint8_t pin, uint16_t duty);
int hal_pwm_set_period_us(uint8_t pin, uint32_t us, uint32_t pulse_us);

int hal_i2c_init(uint8_t unit, uint8_t sda, uint8_t scl, uint32_t hz);
int hal_i2c_write(uint8_t unit, uint8_t addr, const uint8_t *bytes,
                  size_t length, int stop);
int hal_i2c_read(uint8_t unit, uint8_t addr, uint8_t *bytes, size_t length);
int hal_spi_init(uint8_t unit, uint8_t sck, uint8_t mosi, uint8_t miso,
                 uint32_t hz, uint8_t mode, uint8_t bits);
int hal_spi_transfer(uint8_t unit, const uint8_t *tx, uint8_t *rx,
                     size_t length);
int hal_uart_init(uint8_t unit, uint8_t tx, uint8_t rx, uint32_t baud,
                  uint8_t data, uint8_t parity, uint8_t stop);
int hal_uart_write(uint8_t unit, const uint8_t *bytes, size_t length);
int hal_uart_read(uint8_t unit, uint8_t *bytes, size_t length);

uint64_t hal_time_us(void);
void hal_sleep_ms(uint32_t milliseconds);
void hal_deep_sleep_ms(uint32_t milliseconds);
void hal_reset(void);
uint32_t hal_cpu_freq(void);
int hal_unique_id(uint8_t *buffer, size_t length);
uint32_t hal_random(void);

void hal_console_write(const char *bytes, size_t length);
int hal_console_read(uint8_t *bytes, size_t length);

int hal_flash_read(uint32_t offset, void *buffer, size_t length);
int hal_flash_prog(uint32_t offset, const void *buffer, size_t length);
int hal_flash_erase(uint32_t block);

int hal_event_push(uint8_t type, uint8_t id, uint32_t argument);
int hal_event_pop(uint8_t *type, uint8_t *id, uint32_t *argument);

int hal_wdt_enable(uint32_t milliseconds);
void hal_wdt_feed(void);

#endif
