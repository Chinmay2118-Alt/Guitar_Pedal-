#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_adc/adc_oneshot.h" // The modern ESP-IDF ADC driver
#include "dsp.h"

// UART Configuration
#define HIL_UART_PORT UART_NUM_0
#define HIL_BAUD_RATE 921600
#define BUF_SIZE 4096 

// ADC Configuration for GPIO 33
#define POT_ADC_UNIT ADC_UNIT_1
#define POT_ADC_CHAN ADC_CHANNEL_5 

static int16_t audio_buffer[BLOCK_SIZE]; 
adc_oneshot_unit_handle_t adc1_handle; // Global handle so control_task can see it

// ── Core 1: High-Priority Audio Task ────────────────────────
void audio_task(void *pvParameters) {
    const size_t bytes_to_read = BLOCK_SIZE * sizeof(int16_t);
    while (1) {
        int len = uart_read_bytes(HIL_UART_PORT, (uint8_t*)audio_buffer, bytes_to_read, portMAX_DELAY);
        if (len == bytes_to_read) {
            process_block(audio_buffer, audio_buffer, BLOCK_SIZE);
            uart_write_bytes(HIL_UART_PORT, (const char*)audio_buffer, bytes_to_read);
        }
    }
}

// ── Core 0: Low-Priority Control Task (The Physical Knobs) ──
void control_task(void *pvParameters) {
    float smoothed_adc = 0.0f;
    const float alpha = 0.05f; // Heavy smoothing (Low-Pass Filter) for the physical knob

    while (1) {
        int raw_val = 0;
        
        // 1. Read the physical pin (Returns 0 to 4095)
        adc_oneshot_read(adc1_handle, POT_ADC_CHAN, &raw_val);

        // 2. Smooth the jittery electrical signal
        smoothed_adc = (alpha * raw_val) + ((1.0f - alpha) * smoothed_adc);

        // 3. Map the 0-4095 range to our Gain range (1.0f to 50.0f)
        float mapped_gain = 1.0f + (smoothed_adc / 4095.0f) * 49.0f;

        // 4. Atomically update the core DSP math
        g_dist_gain = mapped_gain;

        // Poll at 50Hz (20ms delay) to yield Core 0 to system tasks
        vTaskDelay(pdMS_TO_TICKS(20)); 
    }
}

// ── System Entry Point ──────────────────────────────────────
void app_main(void) {
    init_dsp();

    // --- Configure the Analog-to-Digital Converter ---
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = POT_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_11, // DB_11 sets the hardware range to safely read 0 to ~3.1V
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, POT_ADC_CHAN, &config));

    // --- Configure the High-Speed UART ---
    uart_config_t uart_config = {
        .baud_rate = HIL_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(HIL_UART_PORT, BUF_SIZE, BUF_SIZE, 0, NULL, 0);
    uart_param_config(HIL_UART_PORT, &uart_config);
    uart_set_pin(HIL_UART_PORT, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // --- Launch Dual-Core Tasks ---
    xTaskCreatePinnedToCore(audio_task, "audio_task", 8192, NULL, configMAX_PRIORITIES - 1, NULL, 1);
    xTaskCreatePinnedToCore(control_task, "control_task", 4096, NULL, 5, NULL, 0);
}
