#include "pgems.h"

#include "parray.h"
#include "pclass.h"
#include "pphp/hal.h"

#include <stdint.h>
#include <string.h>

typedef struct pin_data {
    uint8_t pin;
    uint8_t flags;
} pin_data;

typedef struct gpio_data {
    pin_data pin;
    pvalue callback;
} gpio_data;

typedef struct pwm_data {
    uint8_t pin;
    uint32_t frequency;
    uint32_t period;
    uint32_t pulse;
} pwm_data;

typedef struct i2c_data { uint8_t unit; } i2c_data;
typedef struct spi_data { uint8_t unit; uint8_t filler; } spi_data;
typedef struct uart_data { uint8_t unit; } uart_data;

static int rtc_offset;

static int expect_count(pphp_ctx *context, int minimum, int maximum) {
    int count = pphp_argc(context);
    if (count < minimum || count > maximum) {
        return pphp_raise(context, "ArgumentCountError",
                          "native method expects %d to %d arguments, %d given",
                          minimum, maximum, count);
    }
    return 0;
}

static int hal_failed(pphp_ctx *context, const char *operation, int status) {
    if (status >= 0) return 0;
    return pphp_raise(context, "RuntimeException", "%s failed (HAL %d)",
                      operation, status);
}

static void *attach_data(pphp_ctx *context, size_t size,
                         void (*finalizer)(void *)) {
    pobject *object = pphp_this(context);
    if (object == NULL) {
        (void)pphp_raise(context, "Error",
                         "native instance method has no object");
        return NULL;
    }
    if (object->native_data == NULL) {
        object->native_data = pphp_alloc(size);
        if (object->native_data == NULL) {
            (void)pphp_raise(context, "OutOfMemoryError",
                             "out of memory allocating peripheral state");
            return NULL;
        }
        memset(object->native_data, 0, size);
        object->native_finalizer = finalizer;
    }
    return object->native_data;
}

static void gpio_finalize(void *opaque) {
    gpio_data *data = opaque;
    if (data != NULL) pv_release(data->callback);
}

static int gpio_construct(pphp_ctx *context) {
    gpio_data *data;
    pphp_int pin;
    pphp_int flags = 1;
    uint8_t mode;
    uint8_t pull;
    if (expect_count(context, 1, 2) < 0) return -1;
    pin = pphp_arg_int(context, 0);
    if (pphp_argc(context) == 2) flags = pphp_arg_int(context, 1);
    if (pin < 0 || pin > UINT8_MAX) {
        return pphp_raise(context, "ValueError", "GPIO pin is out of range");
    }
    data = attach_data(context, sizeof(*data), gpio_finalize);
    if (data == NULL) return -1;
    data->pin.pin = (uint8_t)pin;
    data->pin.flags = (uint8_t)flags;
    data->callback = pv_null();
    mode = (flags & 2) != 0 ? 2U : 1U;
    pull = (uint8_t)(flags & (8 | 16));
    return hal_failed(context, "GPIO initialization",
                      hal_gpio_init(data->pin.pin, mode, pull));
}

static gpio_data *gpio_this(pphp_ctx *context) {
    gpio_data *data = pphp_obj_data(pphp_this(context));
    if (data == NULL) {
        (void)pphp_raise(context, "RuntimeException",
                         "GPIO is not initialized");
    }
    return data;
}

static int gpio_setmode(pphp_ctx *context) {
    gpio_data *data;
    pphp_int flags;
    if (expect_count(context, 1, 1) < 0) return -1;
    data = gpio_this(context);
    flags = pphp_arg_int(context, 0);
    if (data == NULL) return -1;
    data->pin.flags = (uint8_t)flags;
    if (hal_failed(context, "GPIO mode",
                   hal_gpio_init(data->pin.pin,
                                 (flags & 2) != 0 ? 2U : 1U,
                                 (uint8_t)(flags & (8 | 16)))) < 0) return -1;
    pphp_ret_null(context);
    return 0;
}

static int gpio_read(pphp_ctx *context) {
    gpio_data *data;
    int value;
    if (expect_count(context, 0, 0) < 0) return -1;
    data = gpio_this(context);
    if (data == NULL) return -1;
    value = hal_gpio_read(data->pin.pin);
    if (hal_failed(context, "GPIO read", value) < 0) return -1;
    pphp_ret_int(context, (pphp_int)value);
    return 0;
}

static int gpio_write_level(pphp_ctx *context, int fixed) {
    gpio_data *data;
    pphp_int level = fixed;
    if (fixed < 0) {
        if (expect_count(context, 1, 1) < 0) return -1;
        level = pphp_arg_int(context, 0);
    } else if (expect_count(context, 0, 0) < 0) {
        return -1;
    }
    data = gpio_this(context);
    if (data == NULL) return -1;
    if (hal_failed(context, "GPIO write",
                   hal_gpio_write(data->pin.pin,
                                  (uint8_t)(level == 0 ? 0U : 1U))) < 0) {
        return -1;
    }
    pphp_ret_null(context);
    return 0;
}

static int gpio_high(pphp_ctx *context) { return gpio_write_level(context, 1); }
static int gpio_low(pphp_ctx *context) { return gpio_write_level(context, 0); }
static int gpio_write(pphp_ctx *context) { return gpio_write_level(context, -1); }

static int gpio_irq(pphp_ctx *context) {
    gpio_data *data;
    pvalue callback;
    pphp_int edges;
    if (expect_count(context, 2, 2) < 0) return -1;
    data = gpio_this(context);
    if (data == NULL) return -1;
    callback = pphp_arg(context, 0);
    edges = pphp_arg_int(context, 1);
    if (callback.type != PT_STRING && callback.type != PT_ARRAY &&
        callback.type != PT_CLOSURE && callback.type != PT_OBJECT) {
        return pphp_raise(context, "TypeError",
                          "GPIO irq callback is not callable");
    }
    if (hal_failed(context, "GPIO irq",
                   hal_gpio_irq_enable(data->pin.pin,
                                       (uint8_t)edges)) < 0) return -1;
    pv_retain(callback);
    pv_release(data->callback);
    data->callback = callback;
    pphp_ret_null(context);
    return 0;
}

static int adc_construct(pphp_ctx *context) {
    pin_data *data;
    pphp_int pin;
    if (expect_count(context, 1, 1) < 0) return -1;
    pin = pphp_arg_int(context, 0);
    if (pin < 0 || pin > UINT8_MAX) {
        return pphp_raise(context, "ValueError", "ADC pin is out of range");
    }
    data = attach_data(context, sizeof(*data), NULL);
    if (data == NULL) return -1;
    data->pin = (uint8_t)pin;
    return hal_failed(context, "ADC initialization", hal_adc_init(data->pin));
}

static int adc_sample(pphp_ctx *context, int voltage) {
    pin_data *data = pphp_obj_data(pphp_this(context));
    uint16_t sample;
    if (expect_count(context, 0, 0) < 0) return -1;
    if (data == NULL) return pphp_raise(context, "RuntimeException",
                                        "ADC is not initialized");
    if (hal_failed(context, "ADC read",
                   hal_adc_read_u16(data->pin, &sample)) < 0) return -1;
    if (voltage) {
#if PPHP_ENABLE_FLOAT
        pphp_ret_float(context,
                       (pphp_float)((double)sample * 3.3 / 65536.0));
#else
        return pphp_raise(context, "RuntimeException",
                          "float support disabled");
#endif
    } else {
        pphp_ret_int(context, (pphp_int)sample);
    }
    return 0;
}
static int adc_read_u16(pphp_ctx *context) { return adc_sample(context, 0); }
static int adc_read_voltage(pphp_ctx *context) { return adc_sample(context, 1); }

static int pwm_construct(pphp_ctx *context) {
    pwm_data *data;
    pphp_int pin;
    pphp_int frequency = 1000;
    if (expect_count(context, 1, 2) < 0) return -1;
    pin = pphp_arg_int(context, 0);
    if (pphp_argc(context) == 2) frequency = pphp_arg_int(context, 1);
    if (pin < 0 || pin > UINT8_MAX || frequency <= 0) {
        return pphp_raise(context, "ValueError", "invalid PWM configuration");
    }
    data = attach_data(context, sizeof(*data), NULL);
    if (data == NULL) return -1;
    data->pin = (uint8_t)pin;
    data->frequency = (uint32_t)frequency;
    return hal_failed(context, "PWM initialization",
                      hal_pwm_init(data->pin, data->frequency));
}

static pwm_data *pwm_this(pphp_ctx *context) {
    pwm_data *data = pphp_obj_data(pphp_this(context));
    if (data == NULL) (void)pphp_raise(context, "RuntimeException",
                                       "PWM is not initialized");
    return data;
}

static int pwm_frequency(pphp_ctx *context) {
    pwm_data *data;
    pphp_int frequency;
    if (expect_count(context, 1, 1) < 0) return -1;
    data = pwm_this(context);
    frequency = pphp_arg_int(context, 0);
    if (data == NULL || frequency <= 0) return -1;
    data->frequency = (uint32_t)frequency;
    if (hal_failed(context, "PWM frequency",
                   hal_pwm_init(data->pin, data->frequency)) < 0) return -1;
    pphp_ret_null(context);
    return 0;
}

static int pwm_duty(pphp_ctx *context) {
    pwm_data *data;
    pphp_int duty;
    if (expect_count(context, 1, 1) < 0) return -1;
    data = pwm_this(context);
    duty = pphp_arg_int(context, 0);
    if (data == NULL || duty < 0 || duty > UINT16_MAX) {
        return pphp_raise(context, "ValueError", "PWM duty is out of range");
    }
    if (hal_failed(context, "PWM duty",
                   hal_pwm_set_duty_u16(data->pin, (uint16_t)duty)) < 0) return -1;
    pphp_ret_null(context);
    return 0;
}

static int pwm_period(pphp_ctx *context) {
    pwm_data *data;
    pphp_int period;
    if (expect_count(context, 1, 1) < 0) return -1;
    data = pwm_this(context);
    period = pphp_arg_int(context, 0);
    if (data == NULL || period <= 0) return -1;
    data->period = (uint32_t)period;
    if (hal_failed(context, "PWM period",
                   hal_pwm_set_period_us(data->pin, data->period,
                                         data->pulse)) < 0) return -1;
    pphp_ret_null(context);
    return 0;
}

static int pwm_pulse(pphp_ctx *context) {
    pwm_data *data;
    pphp_int pulse;
    if (expect_count(context, 1, 1) < 0) return -1;
    data = pwm_this(context);
    pulse = pphp_arg_int(context, 0);
    if (data == NULL || pulse < 0) return -1;
    data->pulse = (uint32_t)pulse;
    if (hal_failed(context, "PWM pulse width",
                   hal_pwm_set_period_us(data->pin, data->period,
                                         data->pulse)) < 0) return -1;
    pphp_ret_null(context);
    return 0;
}

static int pwm_stop(pphp_ctx *context) {
    pwm_data *data = pwm_this(context);
    if (expect_count(context, 0, 0) < 0 || data == NULL) return -1;
    if (hal_failed(context, "PWM stop",
                   hal_pwm_set_duty_u16(data->pin, 0U)) < 0) return -1;
    pphp_ret_null(context);
    return 0;
}

static int pwm_start(pphp_ctx *context) {
    pwm_data *data = pwm_this(context);
    if (expect_count(context, 0, 0) < 0 || data == NULL) return -1;
    if (hal_failed(context, "PWM start",
                   hal_pwm_init(data->pin, data->frequency)) < 0) return -1;
    pphp_ret_null(context);
    return 0;
}

static int i2c_construct(pphp_ctx *context) {
    i2c_data *data;
    pphp_int unit, sda, scl, frequency = 100000;
    if (expect_count(context, 3, 4) < 0) return -1;
    unit = pphp_arg_int(context, 0);
    sda = pphp_arg_int(context, 1);
    scl = pphp_arg_int(context, 2);
    if (pphp_argc(context) == 4) frequency = pphp_arg_int(context, 3);
    if (unit < 0 || unit > UINT8_MAX || sda < 0 || sda > UINT8_MAX ||
        scl < 0 || scl > UINT8_MAX || frequency <= 0) {
        return pphp_raise(context, "ValueError", "invalid I2C configuration");
    }
    data = attach_data(context, sizeof(*data), NULL);
    if (data == NULL) return -1;
    data->unit = (uint8_t)unit;
    return hal_failed(context, "I2C initialization",
                      hal_i2c_init(data->unit, (uint8_t)sda, (uint8_t)scl,
                                   (uint32_t)frequency));
}

static i2c_data *i2c_this(pphp_ctx *context) {
    i2c_data *data = pphp_obj_data(pphp_this(context));
    if (data == NULL) (void)pphp_raise(context, "RuntimeException",
                                       "I2C is not initialized");
    return data;
}

static int i2c_write_method(pphp_ctx *context) {
    i2c_data *data;
    pphp_int address;
    const char *bytes;
    size_t length;
    int stop = 1;
    int status;
    if (expect_count(context, 2, 3) < 0) return -1;
    data = i2c_this(context);
    address = pphp_arg_int(context, 0);
    bytes = pphp_arg_str(context, 1, &length);
    if (pphp_argc(context) == 3) stop = pv_is_truthy(pphp_arg(context, 2));
    if (data == NULL || bytes == NULL || address < 0 || address > 127) return -1;
    status = hal_i2c_write(data->unit, (uint8_t)address,
                           (const uint8_t *)bytes, length, stop);
    if (hal_failed(context, "I2C write", status) < 0) return -1;
    pphp_ret_int(context, (pphp_int)status);
    return 0;
}

static int i2c_read_bytes(pphp_ctx *context, int write_first) {
    i2c_data *data;
    pphp_int address;
    pphp_int requested;
    const char *out = NULL;
    size_t out_length = 0U;
    uint8_t *buffer;
    int status;
    if (expect_count(context, write_first ? 3 : 2,
                     write_first ? 3 : 2) < 0) return -1;
    data = i2c_this(context);
    address = pphp_arg_int(context, 0);
    if (write_first) {
        out = pphp_arg_str(context, 1, &out_length);
        requested = pphp_arg_int(context, 2);
    } else {
        requested = pphp_arg_int(context, 1);
    }
    if (data == NULL || address < 0 || address > 127 || requested < 0 ||
        (uint64_t)requested > PPHP_STR_MAX || (write_first && out == NULL)) {
        return -1;
    }
    if (write_first) {
        status = hal_i2c_write(data->unit, (uint8_t)address,
                               (const uint8_t *)out, out_length, 0);
        if (hal_failed(context, "I2C write-read", status) < 0) return -1;
    }
    buffer = pphp_alloc((size_t)requested + 1U);
    if (buffer == NULL) return pphp_raise(context, "OutOfMemoryError",
                                          "out of memory reading I2C");
    status = hal_i2c_read(data->unit, (uint8_t)address, buffer,
                          (size_t)requested);
    if (hal_failed(context, "I2C read", status) < 0) {
        pphp_free(buffer);
        return -1;
    }
    pphp_ret_strn(context, (const char *)buffer, (size_t)requested);
    pphp_free(buffer);
    return 0;
}
static int i2c_read_method(pphp_ctx *context) { return i2c_read_bytes(context, 0); }
static int i2c_write_read(pphp_ctx *context) { return i2c_read_bytes(context, 1); }

static int i2c_scan(pphp_ctx *context) {
    i2c_data *data = i2c_this(context);
    parray *addresses;
    unsigned address;
    if (expect_count(context, 0, 0) < 0 || data == NULL) return -1;
    addresses = pa_new(8U);
    if (addresses == NULL) return pphp_raise(context, "OutOfMemoryError",
                                             "out of memory scanning I2C");
    for (address = 8U; address < 120U; address++) {
        if (hal_i2c_write(data->unit, (uint8_t)address, NULL, 0U, 1) >= 0 &&
            !pa_push(addresses, pv_int((pphp_int)address))) {
            pv_release(pv_heap(PT_ARRAY, &addresses->header));
            return pphp_raise(context, "OutOfMemoryError",
                              "out of memory scanning I2C");
        }
    }
    pphp_ret_value(context, pv_heap(PT_ARRAY, &addresses->header));
    return 0;
}

static int spi_construct(pphp_ctx *context) {
    spi_data *data;
    pphp_int unit, sck, mosi, miso, frequency = 1000000, mode = 0, bits = 8;
    if (expect_count(context, 4, 7) < 0) return -1;
    unit = pphp_arg_int(context, 0); sck = pphp_arg_int(context, 1);
    mosi = pphp_arg_int(context, 2); miso = pphp_arg_int(context, 3);
    if (pphp_argc(context) > 4) frequency = pphp_arg_int(context, 4);
    if (pphp_argc(context) > 5) mode = pphp_arg_int(context, 5);
    if (pphp_argc(context) > 6) bits = pphp_arg_int(context, 6);
    if (unit < 0 || unit > UINT8_MAX || sck < 0 || sck > UINT8_MAX ||
        mosi < 0 || mosi > UINT8_MAX || miso < 0 || miso > UINT8_MAX ||
        frequency <= 0 || mode < 0 || mode > 3 || bits <= 0 || bits > 16) {
        return pphp_raise(context, "ValueError", "invalid SPI configuration");
    }
    data = attach_data(context, sizeof(*data), NULL);
    if (data == NULL) return -1;
    data->unit = (uint8_t)unit;
    return hal_failed(context, "SPI initialization",
                      hal_spi_init(data->unit, (uint8_t)sck, (uint8_t)mosi,
                                   (uint8_t)miso, (uint32_t)frequency,
                                   (uint8_t)mode, (uint8_t)bits));
}

static spi_data *spi_this(pphp_ctx *context) {
    spi_data *data = pphp_obj_data(pphp_this(context));
    if (data == NULL) (void)pphp_raise(context, "RuntimeException",
                                       "SPI is not initialized");
    return data;
}

static int spi_transfer_method(pphp_ctx *context) {
    spi_data *data;
    const char *output;
    size_t length;
    uint8_t *input;
    if (expect_count(context, 1, 1) < 0) return -1;
    data = spi_this(context);
    output = pphp_arg_str(context, 0, &length);
    if (data == NULL || output == NULL) return -1;
    input = pphp_alloc(length + 1U);
    if (input == NULL) return pphp_raise(context, "OutOfMemoryError",
                                         "out of memory transferring SPI");
    if (hal_failed(context, "SPI transfer",
                   hal_spi_transfer(data->unit, (const uint8_t *)output,
                                    input, length)) < 0) {
        pphp_free(input);
        return -1;
    }
    pphp_ret_strn(context, (const char *)input, length);
    pphp_free(input);
    return 0;
}

static int spi_write_method(pphp_ctx *context) {
    spi_data *data;
    const char *output;
    size_t length;
    if (expect_count(context, 1, 1) < 0) return -1;
    data = spi_this(context);
    output = pphp_arg_str(context, 0, &length);
    if (data == NULL || output == NULL) return -1;
    if (hal_failed(context, "SPI write",
                   hal_spi_transfer(data->unit, (const uint8_t *)output,
                                    NULL, length)) < 0) return -1;
    pphp_ret_null(context);
    return 0;
}

static int spi_read_method(pphp_ctx *context) {
    spi_data *data;
    pphp_int requested;
    pphp_int filler = 0;
    uint8_t *output;
    uint8_t *input;
    if (expect_count(context, 1, 2) < 0) return -1;
    data = spi_this(context);
    requested = pphp_arg_int(context, 0);
    if (pphp_argc(context) == 2) filler = pphp_arg_int(context, 1);
    if (data == NULL || requested < 0 || (uint64_t)requested > PPHP_STR_MAX ||
        filler < 0 || filler > UINT8_MAX) return -1;
    output = pphp_alloc((size_t)requested);
    input = pphp_alloc((size_t)requested + 1U);
    if (output == NULL || input == NULL) {
        pphp_free(output); pphp_free(input);
        return pphp_raise(context, "OutOfMemoryError",
                          "out of memory reading SPI");
    }
    memset(output, (int)filler, (size_t)requested);
    if (hal_failed(context, "SPI read",
                   hal_spi_transfer(data->unit, output, input,
                                    (size_t)requested)) < 0) {
        pphp_free(output); pphp_free(input); return -1;
    }
    pphp_ret_strn(context, (const char *)input, (size_t)requested);
    pphp_free(output); pphp_free(input);
    return 0;
}

static int uart_construct(pphp_ctx *context) {
    uart_data *data;
    pphp_int unit, tx, rx, baud = 115200, bits = 8, parity = 0, stop = 1;
    if (expect_count(context, 3, 7) < 0) return -1;
    unit = pphp_arg_int(context, 0); tx = pphp_arg_int(context, 1);
    rx = pphp_arg_int(context, 2);
    if (pphp_argc(context) > 3) baud = pphp_arg_int(context, 3);
    if (pphp_argc(context) > 4) bits = pphp_arg_int(context, 4);
    if (pphp_argc(context) > 5) parity = pphp_arg_int(context, 5);
    if (pphp_argc(context) > 6) stop = pphp_arg_int(context, 6);
    if (unit < 0 || unit > UINT8_MAX || tx < 0 || tx > UINT8_MAX ||
        rx < 0 || rx > UINT8_MAX || baud <= 0) {
        return pphp_raise(context, "ValueError", "invalid UART configuration");
    }
    data = attach_data(context, sizeof(*data), NULL);
    if (data == NULL) return -1;
    data->unit = (uint8_t)unit;
    return hal_failed(context, "UART initialization",
                      hal_uart_init(data->unit, (uint8_t)tx, (uint8_t)rx,
                                    (uint32_t)baud, (uint8_t)bits,
                                    (uint8_t)parity, (uint8_t)stop));
}

static uart_data *uart_this(pphp_ctx *context) {
    uart_data *data = pphp_obj_data(pphp_this(context));
    if (data == NULL) (void)pphp_raise(context, "RuntimeException",
                                       "UART is not initialized");
    return data;
}

static int uart_write_method(pphp_ctx *context) {
    uart_data *data;
    const char *bytes;
    size_t length;
    int status;
    if (expect_count(context, 1, 1) < 0) return -1;
    data = uart_this(context);
    bytes = pphp_arg_str(context, 0, &length);
    if (data == NULL || bytes == NULL) return -1;
    status = hal_uart_write(data->unit, (const uint8_t *)bytes, length);
    if (hal_failed(context, "UART write", status) < 0) return -1;
    pphp_ret_int(context, (pphp_int)status);
    return 0;
}

static int uart_read_method(pphp_ctx *context) {
    uart_data *data;
    pphp_int maximum = 256;
    uint8_t *buffer;
    int count;
    if (expect_count(context, 0, 1) < 0) return -1;
    data = uart_this(context);
    if (pphp_argc(context) == 1) maximum = pphp_arg_int(context, 0);
    if (data == NULL || maximum < 0 || (uint64_t)maximum > PPHP_STR_MAX) return -1;
    buffer = pphp_alloc((size_t)maximum + 1U);
    if (buffer == NULL) return pphp_raise(context, "OutOfMemoryError",
                                          "out of memory reading UART");
    count = hal_uart_read(data->unit, buffer, (size_t)maximum);
    if (hal_failed(context, "UART read", count) < 0) {
        pphp_free(buffer); return -1;
    }
    pphp_ret_strn(context, (const char *)buffer, (size_t)count);
    pphp_free(buffer);
    return 0;
}

static int uart_readline(pphp_ctx *context) {
    uart_data *data;
    pphp_int timeout = -1;
    uint64_t start = hal_time_us();
    char buffer[PPHP_STR_MAX < 256U ? PPHP_STR_MAX : 256U];
    size_t length = 0U;
    if (expect_count(context, 0, 1) < 0) return -1;
    data = uart_this(context);
    if (pphp_argc(context) == 1) timeout = pphp_arg_int(context, 0);
    if (data == NULL) return -1;
    while (length < sizeof(buffer)) {
        int count = hal_uart_read(data->unit, (uint8_t *)buffer + length, 1U);
        if (count < 0) return hal_failed(context, "UART readline", count);
        if (count == 1) {
            if (buffer[length++] == '\n') break;
        } else if (timeout >= 0 &&
                   hal_time_us() - start >= (uint64_t)timeout * 1000U) {
            break;
        } else if (timeout == 0) {
            break;
        } else {
            hal_sleep_ms(1U);
        }
    }
    if (length == 0U && timeout >= 0) {
        pphp_ret_null(context);
    } else {
        pphp_ret_strn(context, buffer, length);
    }
    return 0;
}

static int uart_available(pphp_ctx *context) {
    if (expect_count(context, 0, 0) < 0 || uart_this(context) == NULL) return -1;
    pphp_ret_int(context, 0);
    return 0;
}
static int uart_flush(pphp_ctx *context) {
    if (expect_count(context, 0, 0) < 0 || uart_this(context) == NULL) return -1;
    pphp_ret_null(context);
    return 0;
}

static int machine_sleep(pphp_ctx *context, int deep) {
    pphp_int milliseconds;
    if (expect_count(context, 1, 1) < 0) return -1;
    milliseconds = pphp_arg_int(context, 0);
    if (milliseconds < 0) return pphp_raise(context, "ValueError",
                                             "sleep duration must be non-negative");
    if (deep) hal_deep_sleep_ms((uint32_t)milliseconds);
    else hal_sleep_ms((uint32_t)milliseconds);
    pphp_ret_null(context);
    return 0;
}
static int machine_sleep_ms(pphp_ctx *context) { return machine_sleep(context, 0); }
static int machine_deep_sleep_ms(pphp_ctx *context) { return machine_sleep(context, 1); }
static int machine_reset(pphp_ctx *context) {
    if (expect_count(context, 0, 0) < 0) return -1;
    hal_reset(); pphp_ret_null(context); return 0;
}
static int machine_unique_id(pphp_ctx *context) {
    uint8_t id[8];
    char hex[16];
    static const char digits[] = "0123456789abcdef";
    size_t i;
    if (expect_count(context, 0, 0) < 0) return -1;
    if (hal_failed(context, "unique id", hal_unique_id(id, sizeof(id))) < 0) return -1;
    for (i = 0U; i < sizeof(id); i++) {
        hex[i * 2U] = digits[id[i] >> 4U];
        hex[i * 2U + 1U] = digits[id[i] & 15U];
    }
    pphp_ret_strn(context, hex, sizeof(hex));
    return 0;
}
static int machine_freq(pphp_ctx *context) {
    if (expect_count(context, 0, 0) < 0) return -1;
    pphp_ret_int(context, (pphp_int)hal_cpu_freq()); return 0;
}
static int machine_tick_us(pphp_ctx *context) {
    uint64_t ticks;
    if (expect_count(context, 0, 0) < 0) return -1;
    ticks = hal_time_us();
#if PPHP_INT64
    pphp_ret_int(context, (pphp_int)ticks);
#else
    pphp_ret_int(context, (pphp_int)(ticks & UINT32_MAX));
#endif
    return 0;
}

static int watchdog_enable(pphp_ctx *context) {
    pphp_int milliseconds;
    if (expect_count(context, 1, 1) < 0) return -1;
    milliseconds = pphp_arg_int(context, 0);
    if (milliseconds <= 0) return pphp_raise(context, "ValueError",
                                              "watchdog timeout must be positive");
    if (hal_failed(context, "watchdog enable",
                   hal_wdt_enable((uint32_t)milliseconds)) < 0) return -1;
    pphp_ret_null(context); return 0;
}
static int watchdog_feed(pphp_ctx *context) {
    if (expect_count(context, 0, 0) < 0) return -1;
    hal_wdt_feed(); pphp_ret_null(context); return 0;
}

static int rtc_set(pphp_ctx *context) {
    pphp_int timestamp;
    if (expect_count(context, 1, 1) < 0) return -1;
    timestamp = pphp_arg_int(context, 0);
    rtc_offset = (int)(timestamp -
                       (pphp_int)(hal_time_us() / UINT64_C(1000000)));
    pphp_ret_null(context); return 0;
}
static int rtc_now(pphp_ctx *context) {
    if (expect_count(context, 0, 0) < 0) return -1;
    pphp_ret_int(context,
                 (pphp_int)(hal_time_us() / UINT64_C(1000000)) + rtc_offset);
    return 0;
}

static pclass *define_final_class(pphp_state *state, const char *name) {
    pclass *class_entry = pphp_def_class(state, name, NULL);
    if (class_entry != NULL) class_entry->flags |= PC_FINAL;
    return class_entry;
}

int pphp_init_pgems(pphp_state *state) {
#if PPHP_ENABLE_PGEMS
    pclass *gpio = define_final_class(state, "GPIO");
    pclass *adc = define_final_class(state, "ADC");
    pclass *pwm = define_final_class(state, "PWM");
    pclass *i2c = define_final_class(state, "I2C");
    pclass *spi = define_final_class(state, "SPI");
    pclass *uart = define_final_class(state, "UART");
    pclass *machine = define_final_class(state, "Machine");
    pclass *watchdog = define_final_class(state, "Watchdog");
    pclass *rtc = define_final_class(state, "RTC");
    if (gpio == NULL || adc == NULL || pwm == NULL || i2c == NULL ||
        spi == NULL || uart == NULL || machine == NULL || watchdog == NULL ||
        rtc == NULL) return 0;
    pphp_def_cconst_int(gpio, "IN", 1); pphp_def_cconst_int(gpio, "OUT", 2);
    pphp_def_cconst_int(gpio, "HIGH_Z", 4); pphp_def_cconst_int(gpio, "PULL_UP", 8);
    pphp_def_cconst_int(gpio, "PULL_DOWN", 16); pphp_def_cconst_int(gpio, "OPEN_DRAIN", 32);
    pphp_def_cconst_int(gpio, "EDGE_RISE", 1); pphp_def_cconst_int(gpio, "EDGE_FALL", 2);
    pphp_def_cconst_int(gpio, "LEVEL_HIGH", 4); pphp_def_cconst_int(gpio, "LEVEL_LOW", 8);
    pphp_def_method(gpio, "__construct", gpio_construct, PPHP_PUBLIC);
    pphp_def_method(gpio, "setmode", gpio_setmode, PPHP_PUBLIC);
    pphp_def_method(gpio, "read", gpio_read, PPHP_PUBLIC);
    pphp_def_method(gpio, "high", gpio_high, PPHP_PUBLIC);
    pphp_def_method(gpio, "low", gpio_low, PPHP_PUBLIC);
    pphp_def_method(gpio, "write", gpio_write, PPHP_PUBLIC);
    pphp_def_method(gpio, "irq", gpio_irq, PPHP_PUBLIC);
    pphp_def_method(adc, "__construct", adc_construct, PPHP_PUBLIC);
    pphp_def_method(adc, "read_u16", adc_read_u16, PPHP_PUBLIC);
    pphp_def_method(adc, "read_voltage", adc_read_voltage, PPHP_PUBLIC);
    pphp_def_method(pwm, "__construct", pwm_construct, PPHP_PUBLIC);
    pphp_def_method(pwm, "frequency", pwm_frequency, PPHP_PUBLIC);
    pphp_def_method(pwm, "duty_u16", pwm_duty, PPHP_PUBLIC);
    pphp_def_method(pwm, "period_us", pwm_period, PPHP_PUBLIC);
    pphp_def_method(pwm, "pulse_width_us", pwm_pulse, PPHP_PUBLIC);
    pphp_def_method(pwm, "stop", pwm_stop, PPHP_PUBLIC);
    pphp_def_method(pwm, "start", pwm_start, PPHP_PUBLIC);
    pphp_def_method(i2c, "__construct", i2c_construct, PPHP_PUBLIC);
    pphp_def_method(i2c, "write", i2c_write_method, PPHP_PUBLIC);
    pphp_def_method(i2c, "read", i2c_read_method, PPHP_PUBLIC);
    pphp_def_method(i2c, "write_read", i2c_write_read, PPHP_PUBLIC);
    pphp_def_method(i2c, "scan", i2c_scan, PPHP_PUBLIC);
    pphp_def_method(spi, "__construct", spi_construct, PPHP_PUBLIC);
    pphp_def_method(spi, "transfer", spi_transfer_method, PPHP_PUBLIC);
    pphp_def_method(spi, "write", spi_write_method, PPHP_PUBLIC);
    pphp_def_method(spi, "read", spi_read_method, PPHP_PUBLIC);
    pphp_def_method(uart, "__construct", uart_construct, PPHP_PUBLIC);
    pphp_def_method(uart, "write", uart_write_method, PPHP_PUBLIC);
    pphp_def_method(uart, "read", uart_read_method, PPHP_PUBLIC);
    pphp_def_method(uart, "readline", uart_readline, PPHP_PUBLIC);
    pphp_def_method(uart, "bytes_available", uart_available, PPHP_PUBLIC);
    pphp_def_method(uart, "flush", uart_flush, PPHP_PUBLIC);
    pphp_def_method(machine, "sleep_ms", machine_sleep_ms, PPHP_PUBLIC | PPHP_STATIC);
    pphp_def_method(machine, "deep_sleep_ms", machine_deep_sleep_ms, PPHP_PUBLIC | PPHP_STATIC);
    pphp_def_method(machine, "reset", machine_reset, PPHP_PUBLIC | PPHP_STATIC);
    pphp_def_method(machine, "unique_id", machine_unique_id, PPHP_PUBLIC | PPHP_STATIC);
    pphp_def_method(machine, "freq", machine_freq, PPHP_PUBLIC | PPHP_STATIC);
    pphp_def_method(machine, "tick_us", machine_tick_us, PPHP_PUBLIC | PPHP_STATIC);
    pphp_def_method(watchdog, "enable", watchdog_enable, PPHP_PUBLIC | PPHP_STATIC);
    pphp_def_method(watchdog, "feed", watchdog_feed, PPHP_PUBLIC | PPHP_STATIC);
    pphp_def_method(rtc, "set", rtc_set, PPHP_PUBLIC | PPHP_STATIC);
    pphp_def_method(rtc, "now", rtc_now, PPHP_PUBLIC | PPHP_STATIC);
#else
    (void)state;
#endif
    return 1;
}

void pphp_poll_pgems(pphp_state *state) {
#if PPHP_ENABLE_PGEMS
    uint8_t type;
    uint8_t id;
    uint32_t argument;
    while (state != NULL && hal_event_pop(&type, &id, &argument) > 0) {
        pobject *object;
        if (type != 1U) continue;
        for (object = state->gc_objects; object != NULL;
             object = object->gc_next) {
            gpio_data *data;
            pvalue result = pv_null();
            pvalue callback_argument = pv_int((pphp_int)argument);
            if (!ps_equal_bytes(object->class_entry->name, "GPIO", 4U)) continue;
            data = object->native_data;
            if (data == NULL || data->pin.pin != id ||
                data->callback.type == PT_NULL) continue;
            if (state->invoke != NULL) {
                (void)state->invoke(state, data->callback, &callback_argument,
                                    1U, &result);
                pv_release(result);
            }
        }
    }
#else
    (void)state;
#endif
}
