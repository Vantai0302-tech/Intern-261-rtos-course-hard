#include "FreeRTOS.h"
#include "task.h"
#include "gpio.h"
#include "blinky.h"
#include "include/params.h"

static void blink(TickType_t period, unsigned probe_pin)
{
    TickType_t last = xTaskGetTickCount();

    for (;;) {
        gpio_toggle(W1E1_TAG_GPIO_PORT, W1E1_TAG_GPIO_PIN);
        gpio_toggle(GPIOA, probe_pin);
        (void)xTaskDelayUntil(&last, period);
    }
}

void vTaskBlinkA(void *pvParameters)
{
    (void)pvParameters;
    blink(pdMS_TO_TICKS(W1E1_TASK_A_PERIOD_MS), 1U);
}

void vTaskBlinkB(void *pvParameters)
{
    (void)pvParameters;
    blink(pdMS_TO_TICKS(W1E1_TASK_B_PERIOD_MS), 2U);
}

void blinky_app_init(void)
{
    gpio_mode(W1E1_TAG_GPIO_PORT, W1E1_TAG_GPIO_PIN, 2U);
    gpio_write(W1E1_TAG_GPIO_PORT, W1E1_TAG_GPIO_PIN, 0U);
    gpio_mode(GPIOA, 1U, 2U);
    gpio_mode(GPIOA, 2U, 2U);
    gpio_write(GPIOA, 1U, 0U);
    gpio_write(GPIOA, 2U, 0U);

    BaseType_t a = xTaskCreate(vTaskBlinkA, "blinkA", 192U, NULL, 1U, NULL);
    BaseType_t b = xTaskCreate(vTaskBlinkB, "blinkB", 192U, NULL, 1U, NULL);

    configASSERT(a == pdPASS && b == pdPASS);
}
