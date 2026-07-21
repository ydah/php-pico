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

typedef struct gpio_state {
    uint8_t initialized;
    uint8_t mode;
    uint8_t pull;
    uint8_t level;
    uint8_t edges;
} gpio_state;

typedef struct pwm_state {
    uint32_t frequency;
    uint32_t period;
    uint32_t pulse;
    uint16_t duty;
    uint8_t initialized;
} pwm_state;

static hal_event events[16];
static unsigned event_read;
static unsigned event_write;
static uint32_t random_state = UINT32_C(0x9e3779b9);
static gpio_state gpio_pins[256];
static pwm_state pwm_pins[256];
static uint8_t adc_pins[256];
static uint8_t i2c_units[2];
static uint8_t spi_units[2];
static uint8_t uart_units[2];
static uint8_t flash_image[1024U * 1024U];

int hal_init(void) {
    struct timeval now;
    if (gettimeofday(&now, NULL) == 0) {
        random_state ^= (uint32_t)now.tv_sec ^ (uint32_t)now.tv_usec;
    }
    return PPHP_HAL_OK;
}

int hal_gpio_init(uint8_t pin, uint8_t mode, uint8_t pull) {
    gpio_pins[pin].initialized = 1U;
    gpio_pins[pin].mode = mode;
    gpio_pins[pin].pull = pull;
    return PPHP_HAL_OK;
}
int hal_gpio_write(uint8_t pin, uint8_t level) {
    if (!gpio_pins[pin].initialized) return PPHP_HAL_INVALID;
    gpio_pins[pin].level = level == 0U ? 0U : 1U;
    return PPHP_HAL_OK;
}
int hal_gpio_read(uint8_t pin) {
    if (!gpio_pins[pin].initialized) return PPHP_HAL_INVALID;
    return gpio_pins[pin].level;
}
int hal_gpio_irq_enable(uint8_t pin, uint8_t edges) {
    if (!gpio_pins[pin].initialized) return PPHP_HAL_INVALID;
    gpio_pins[pin].edges = edges;
    return PPHP_HAL_OK;
}
int hal_adc_init(uint8_t pin) { adc_pins[pin] = 1U; return PPHP_HAL_OK; }
int hal_adc_read_u16(uint8_t pin, uint16_t *out) {
    if (!adc_pins[pin] || out == NULL) return PPHP_HAL_INVALID;
    *out = (uint16_t)((uint16_t)pin * UINT16_C(257));
    return PPHP_HAL_OK;
}
int hal_pwm_init(uint8_t pin, uint32_t hz) {
    if (hz == 0U) return PPHP_HAL_INVALID;
    pwm_pins[pin].initialized = 1U;
    pwm_pins[pin].frequency = hz;
    return PPHP_HAL_OK;
}
int hal_pwm_set_duty_u16(uint8_t pin, uint16_t duty) {
    if (!pwm_pins[pin].initialized) return PPHP_HAL_INVALID;
    pwm_pins[pin].duty = duty;
    return PPHP_HAL_OK;
}
int hal_pwm_set_period_us(uint8_t pin, uint32_t us, uint32_t pulse_us) {
    if (!pwm_pins[pin].initialized || pulse_us > us) return PPHP_HAL_INVALID;
    pwm_pins[pin].period = us;
    pwm_pins[pin].pulse = pulse_us;
    return PPHP_HAL_OK;
}
int hal_i2c_init(uint8_t unit, uint8_t sda, uint8_t scl, uint32_t hz) {
    (void)sda; (void)scl;
    if (unit >= 2U || hz == 0U) return PPHP_HAL_INVALID;
    i2c_units[unit] = 1U;
    return PPHP_HAL_OK;
}
int hal_i2c_write(uint8_t unit, uint8_t addr, const uint8_t *bytes,
                  size_t length, int stop) {
    (void)addr; (void)bytes; (void)length; (void)stop;
    return unit < 2U && i2c_units[unit] ? PPHP_HAL_ERROR
                                        : PPHP_HAL_INVALID;
}
int hal_i2c_read(uint8_t unit, uint8_t addr, uint8_t *bytes, size_t length) {
    (void)addr; (void)bytes; (void)length;
    return unit < 2U && i2c_units[unit] ? PPHP_HAL_ERROR
                                        : PPHP_HAL_INVALID;
}
int hal_spi_init(uint8_t unit, uint8_t sck, uint8_t mosi, uint8_t miso,
                 uint32_t hz, uint8_t mode, uint8_t bits) {
    (void)sck; (void)mosi; (void)miso; (void)mode; (void)bits;
    if (unit >= 2U || hz == 0U) return PPHP_HAL_INVALID;
    spi_units[unit] = 1U;
    return PPHP_HAL_OK;
}
int hal_spi_transfer(uint8_t unit, const uint8_t *tx, uint8_t *rx,
                     size_t length) {
    size_t i;
    if (unit >= 2U || !spi_units[unit]) return PPHP_HAL_INVALID;
    if (rx != NULL) {
        for (i = 0U; i < length; i++) rx[i] = tx == NULL ? 0U : tx[i];
    }
    return (int)length;
}
int hal_uart_init(uint8_t unit, uint8_t tx, uint8_t rx, uint32_t baud,
                  uint8_t data, uint8_t parity, uint8_t stop) {
    (void)tx; (void)rx; (void)data; (void)parity; (void)stop;
    if (unit >= 2U || baud == 0U) return PPHP_HAL_INVALID;
    uart_units[unit] = 1U;
    return PPHP_HAL_OK;
}
int hal_uart_write(uint8_t unit, const uint8_t *bytes, size_t length) {
    if (unit >= 2U || !uart_units[unit] ||
        (length != 0U && bytes == NULL)) return PPHP_HAL_INVALID;
    return (int)length;
}
int hal_uart_read(uint8_t unit, uint8_t *bytes, size_t length) {
    (void)bytes; (void)length;
    return unit < 2U && uart_units[unit] ? 0 : PPHP_HAL_INVALID;
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
uint32_t hal_cpu_freq(void) { return UINT32_C(125000000); }

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
    if (buffer == NULL || offset > sizeof(flash_image) ||
        length > sizeof(flash_image) - offset) return PPHP_HAL_INVALID;
    memcpy(buffer, flash_image + offset, length);
    return PPHP_HAL_OK;
}
int hal_flash_prog(uint32_t offset, const void *buffer, size_t length) {
    if (buffer == NULL || offset > sizeof(flash_image) ||
        length > sizeof(flash_image) - offset) return PPHP_HAL_INVALID;
    memcpy(flash_image + offset, buffer, length);
    return PPHP_HAL_OK;
}
int hal_flash_erase(uint32_t block) {
    uint32_t offset = block * UINT32_C(4096);
    if (offset >= sizeof(flash_image)) return PPHP_HAL_INVALID;
    memset(flash_image + offset, 0xff, 4096U);
    return PPHP_HAL_OK;
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
    return milliseconds == 0U ? PPHP_HAL_INVALID : PPHP_HAL_OK;
}
void hal_wdt_feed(void) {}
