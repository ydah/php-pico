#define _POSIX_C_SOURCE 200809L

#include "pphp/hal.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

typedef struct hal_event {
    uint8_t type;
    uint8_t id;
    uint32_t argument;
} hal_event;

static hal_event events[16];
static unsigned event_read;
static unsigned event_write;
static uint32_t random_state = UINT32_C(0x9e3779b9);

int hal_init(void) {
    struct timeval now;
    if (gettimeofday(&now, NULL) == 0) {
        random_state ^= (uint32_t)now.tv_sec ^ (uint32_t)now.tv_usec;
    }
    return PPHP_HAL_OK;
}

int hal_gpio_init(uint8_t pin, uint8_t mode, uint8_t pull) {
    (void)pin; (void)mode; (void)pull;
    return PPHP_HAL_UNSUPPORTED;
}
int hal_gpio_write(uint8_t pin, uint8_t level) {
    (void)pin; (void)level;
    return PPHP_HAL_UNSUPPORTED;
}
int hal_gpio_read(uint8_t pin) {
    (void)pin;
    return PPHP_HAL_UNSUPPORTED;
}
int hal_gpio_irq_enable(uint8_t pin, uint8_t edges) {
    (void)pin; (void)edges;
    return PPHP_HAL_UNSUPPORTED;
}
int hal_adc_init(uint8_t pin) { (void)pin; return PPHP_HAL_UNSUPPORTED; }
int hal_adc_read_u16(uint8_t pin, uint16_t *out) {
    (void)pin; (void)out;
    return PPHP_HAL_UNSUPPORTED;
}
int hal_pwm_init(uint8_t pin, uint32_t hz) {
    (void)pin; (void)hz;
    return PPHP_HAL_UNSUPPORTED;
}
int hal_pwm_set_duty_u16(uint8_t pin, uint16_t duty) {
    (void)pin; (void)duty;
    return PPHP_HAL_UNSUPPORTED;
}
int hal_pwm_set_period_us(uint8_t pin, uint32_t us, uint32_t pulse_us) {
    (void)pin; (void)us; (void)pulse_us;
    return PPHP_HAL_UNSUPPORTED;
}
int hal_i2c_init(uint8_t unit, uint8_t sda, uint8_t scl, uint32_t hz) {
    (void)unit; (void)sda; (void)scl; (void)hz;
    return PPHP_HAL_UNSUPPORTED;
}
int hal_i2c_write(uint8_t unit, uint8_t addr, const uint8_t *bytes,
                  size_t length, int stop) {
    (void)unit; (void)addr; (void)bytes; (void)length; (void)stop;
    return PPHP_HAL_UNSUPPORTED;
}
int hal_i2c_read(uint8_t unit, uint8_t addr, uint8_t *bytes, size_t length) {
    (void)unit; (void)addr; (void)bytes; (void)length;
    return PPHP_HAL_UNSUPPORTED;
}
int hal_spi_init(uint8_t unit, uint8_t sck, uint8_t mosi, uint8_t miso,
                 uint32_t hz, uint8_t mode, uint8_t bits) {
    (void)unit; (void)sck; (void)mosi; (void)miso;
    (void)hz; (void)mode; (void)bits;
    return PPHP_HAL_UNSUPPORTED;
}
int hal_spi_transfer(uint8_t unit, const uint8_t *tx, uint8_t *rx,
                     size_t length) {
    (void)unit; (void)tx; (void)rx; (void)length;
    return PPHP_HAL_UNSUPPORTED;
}
int hal_uart_init(uint8_t unit, uint8_t tx, uint8_t rx, uint32_t baud,
                  uint8_t data, uint8_t parity, uint8_t stop) {
    (void)unit; (void)tx; (void)rx; (void)baud;
    (void)data; (void)parity; (void)stop;
    return PPHP_HAL_UNSUPPORTED;
}
int hal_uart_write(uint8_t unit, const uint8_t *bytes, size_t length) {
    (void)unit; (void)bytes; (void)length;
    return PPHP_HAL_UNSUPPORTED;
}
int hal_uart_read(uint8_t unit, uint8_t *bytes, size_t length) {
    (void)unit; (void)bytes; (void)length;
    return PPHP_HAL_UNSUPPORTED;
}

uint64_t hal_time_us(void) {
    struct timeval now;
    if (gettimeofday(&now, NULL) != 0) return 0U;
    return (uint64_t)now.tv_sec * UINT64_C(1000000) + (uint64_t)now.tv_usec;
}

void hal_sleep_ms(uint32_t milliseconds) {
    struct timeval timeout;
    timeout.tv_sec = (long)(milliseconds / 1000U);
    timeout.tv_usec = (int)((milliseconds % 1000U) * 1000U);
    while (select(0, NULL, NULL, NULL, &timeout) < 0 && errno == EINTR) {
        timeout.tv_sec = 0L;
        timeout.tv_usec = 0L;
    }
}

void hal_deep_sleep_ms(uint32_t milliseconds) { hal_sleep_ms(milliseconds); }
void hal_reset(void) {}
uint32_t hal_cpu_freq(void) { return 0U; }

int hal_unique_id(uint8_t *buffer, size_t length) {
    char hostname[64];
    size_t i;
    uint32_t hash = UINT32_C(2166136261);
    if (buffer == NULL) return PPHP_HAL_INVALID;
    if (gethostname(hostname, sizeof(hostname)) != 0) return PPHP_HAL_ERROR;
    hostname[sizeof(hostname) - 1U] = '\0';
    for (i = 0U; hostname[i] != '\0'; i++) {
        hash ^= (uint8_t)hostname[i];
        hash *= UINT32_C(16777619);
    }
    for (i = 0U; i < length; i++) {
        hash ^= hash << 13U;
        hash ^= hash >> 17U;
        hash ^= hash << 5U;
        buffer[i] = (uint8_t)hash;
    }
    return PPHP_HAL_OK;
}

uint32_t hal_random(void) {
    random_state ^= random_state << 13U;
    random_state ^= random_state >> 17U;
    random_state ^= random_state << 5U;
    return random_state;
}

void hal_console_write(const char *bytes, size_t length) {
    if (bytes != NULL && length != 0U) (void)fwrite(bytes, 1U, length, stderr);
}

int hal_console_read(uint8_t *bytes, size_t length) {
    (void)bytes; (void)length;
    return 0;
}

int hal_flash_read(uint32_t offset, void *buffer, size_t length) {
    (void)offset; (void)buffer; (void)length;
    return PPHP_HAL_UNSUPPORTED;
}
int hal_flash_prog(uint32_t offset, const void *buffer, size_t length) {
    (void)offset; (void)buffer; (void)length;
    return PPHP_HAL_UNSUPPORTED;
}
int hal_flash_erase(uint32_t block) {
    (void)block;
    return PPHP_HAL_UNSUPPORTED;
}

int hal_event_push(uint8_t type, uint8_t id, uint32_t argument) {
    unsigned next = (event_write + 1U) % 16U;
    if (next == event_read) return PPHP_HAL_ERROR;
    events[event_write].type = type;
    events[event_write].id = id;
    events[event_write].argument = argument;
    event_write = next;
    return PPHP_HAL_OK;
}

int hal_event_pop(uint8_t *type, uint8_t *id, uint32_t *argument) {
    if (event_read == event_write) return 0;
    if (type != NULL) *type = events[event_read].type;
    if (id != NULL) *id = events[event_read].id;
    if (argument != NULL) *argument = events[event_read].argument;
    event_read = (event_read + 1U) % 16U;
    return 1;
}

int hal_wdt_enable(uint32_t milliseconds) {
    (void)milliseconds;
    return PPHP_HAL_UNSUPPORTED;
}
void hal_wdt_feed(void) {}
