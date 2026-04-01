#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "dsp.h"

// Hardware-in-the-Loop UART configuration
#define HIL_UART_PORT UART_NUM_0
#define HIL_BAUD_RATE 921600
#define BUF_SIZE 4096 // Internal ring buffer size for the UART driver

// Static allocation for real-time memory predictability
// 64 samples * 2 bytes (int16) = 128 bytes total
static int16_t audio_buffer[BLOCK_SIZE]; 

// ── Core 1: High-Priority Audio Task ────────────────────────
void audio_task(void *pvParameters) {
    const size_t bytes_to_read = BLOCK_SIZE * sizeof(int16_t);

    while (1) {
        // 1. Block until exactly 64 samples (128 bytes) arrive from the PC
        int len = uart_read_bytes(HIL_UART_PORT, (uint8_t*)audio_buffer, bytes_to_read, portMAX_DELAY);

        if (len == bytes_to_read) {
            // 2. Process the block through the MXR DSP pipeline
            process_block(audio_buffer, audio_buffer, BLOCK_SIZE);

            // 3. Fire the processed 128 bytes back to the PC
            uart_write_bytes(HIL_UART_PORT, (const char*)audio_buffer, bytes_to_read);
        }
    }
}

// ── Core 0: Low-Priority Control Task ───────────────────────

// Inside main.c -> control_task
void control_task(void *pvParameters) {
    while (1) {
        // Lower gain (3.0f - 5.0f) provides a "crunch" rather than a "buzz"
        g_dist_gain = 4.5f; 
        g_vol_level = 0.7f; 

        vTaskDelay(pdMS_TO_TICKS(50)); 
    }
}

// ── System Entry Point ──────────────────────────────────────
void app_main(void) {
    // 1. Initialize DSP states to zero to prevent startup clicks
    init_dsp();

    // 2. Configure High-Speed UART for audio streaming
    uart_config_t uart_config = {
        .baud_rate = HIL_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    
    // Install driver and configure pins (using default TX/RX for UART0)
    uart_driver_install(HIL_UART_PORT, BUF_SIZE, BUF_SIZE, 0, NULL, 0);
    uart_param_config(HIL_UART_PORT, &uart_config);
    uart_set_pin(HIL_UART_PORT, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // 3. Launch FreeRTOS Tasks
    // Audio task gets absolute highest priority (configMAX_PRIORITIES - 1) on Core 1
    xTaskCreatePinnedToCore(audio_task, "audio_task", 8192, NULL, configMAX_PRIORITIES - 1, NULL, 1);
    
    // Control task gets low priority on Core 0
    xTaskCreatePinnedToCore(control_task, "control_task", 4096, NULL, 5, NULL, 0);
}
