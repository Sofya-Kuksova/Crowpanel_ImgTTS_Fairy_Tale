#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "HimaxModule.h"
#include "i2c_comm/crc_table.h"
#include "i2c_comm/i2c_master.h"

#define HIMAX_RESET_PIN GPIO_NUM_0

#define STATUS_HM_IDLE BIT0
#define STATUS_HM_BUSY BIT1

extern bool i2c_bus_lock(int timeout_ms);
extern bool i2c_bus_unlock(void);

void HimaxModule::task(void* arg)
{
    HimaxModule* himax_module = static_cast<HimaxModule*>(arg);
    esp_err_t err;
    size_t reset_counter = 0;

    while (1) {
        if (xEventGroupWaitBits(himax_module->status_, STATUS_HM_BUSY, pdFALSE, pdFALSE, portMAX_DELAY) &
            STATUS_HM_BUSY) {
            size_t packet_counter            = 0;
            size_t corrupted_packets_counter = 0;
            uint8_t status;
            bool stop = false;
            while (! stop) {
                size_t bursted_packets = 0;
                status                 = himax_module->dev_get_status();
                switch (status) {
                case ST_OUTPUT_RDY: {
                    while (1) {
                        transaction_t tr = {.cmd = CMD_SEND_DATA, .data_length = 0};
                        data_packet_t data;

                        i2c_bus_lock(-1);
                        err = i2c_master_send((void*)&tr, sizeof(transaction_t), pdMS_TO_TICKS(1000));
                        err = i2c_master_recv((void*)&data, sizeof(data_packet_t), pdMS_TO_TICKS(1000));
                        i2c_bus_unlock();

                        uint16_t crc = crc16_compute(crc16_lut, &crc16_ccitt_false_config, (const uint8_t*)data.data,
                                                     data.data_length);
                        ESP_LOGV(TAG, "rx packet: i2c_err=%d, err=%u, idx=%u, total=%u, length=%u, crc=%u(%u)", err,
                                 data.error, data.index, data.total, data.data_length, data.crc, crc);
                        if (err == ESP_ERR_TIMEOUT) {
                            ESP_LOGI(TAG, "Reset device: %u", reset_counter++);
                            himax_module->dev_reset();
                            stop = true;
                            break;
                        }
                        if (err != ESP_OK || data.error != ERR_OK) {
                            break;
                        }
                        if (data.crc != crc) {
                            corrupted_packets_counter++;
                            break;
                        }
                        bursted_packets++;
                        packet_counter++;
                        if (himax_module->player_) {
                            himax_module->player_->write(data.data, data.data_length);
                            if (! himax_module->player_->isOn() && packet_counter > kPlayerStartFramesThreshold) {
                                himax_module->player_->control(1);
                            }

                            if (himax_module->player_->getBufferUtilization() > 0.8f) {
                                break;
                            }
                        }
                        ESP_LOG_BUFFER_HEXDUMP(TAG, &data, sizeof(data_packet_t), ESP_LOG_VERBOSE);
                    }
                } break;
                case ST_IDLE:
                    stop = true;
                    break;
                default:
                    break;
                }
                if (bursted_packets > 0) {
                    ESP_LOGV(TAG, "Bursted %u packets", bursted_packets);
                }
#define BASELINE_DELAY 100
#define DELAY_COEFF    100
#define TARGET_UTIL    0.5
                float util = himax_module->player_->getBufferUtilization();
                int delay  = BASELINE_DELAY + DELAY_COEFF * (util - TARGET_UTIL);
                delay      = std::clamp(delay, 0, BASELINE_DELAY * 2);
                ESP_LOGV(TAG, "rb: util=%f, delay=%u", util, delay);
                vTaskDelay(pdMS_TO_TICKS(delay));
            }
            if (packet_counter > 0) {
                ESP_LOGV(TAG, "Recieved %u packets", packet_counter);
            }
            if (corrupted_packets_counter > 0) {
                ESP_LOGW(TAG, "Recieved %u corrupted packets", corrupted_packets_counter);
            }
            himax_module->player_->control(0);
            xEventGroupClearBits(himax_module->status_, STATUS_HM_BUSY);
        }
    }
}

int HimaxModule::sendText(const char* str, size_t xTicksToWait)
{
    if (xEventGroupGetBits(status_) & STATUS_HM_BUSY) {
        return -1;
    }

    esp_err_t err;
    size_t n = strlen(str);
    ESP_LOGI(TAG, "send str=\"%s\", len=%u", str, n);

    transaction_t tr = {.cmd = CMD_RECV_MSG, .data_length = static_cast<uint16_t>(n)};

    i2c_bus_lock(-1);
    if (!i2c_master_dev_available()) {
        i2c_bus_unlock();
        ESP_LOGW(TAG, "Himax addon is not available on bus");
        return -1;
    }
    err = i2c_master_send((void*)&tr, sizeof(transaction_t), pdMS_TO_TICKS(1000));
    err = i2c_master_send((void*)str, n, pdMS_TO_TICKS(1000));
    i2c_bus_unlock();

    xEventGroupSetBits(status_, STATUS_HM_BUSY);
    return 0;
}

bool HimaxModule::init(AudioPlayer* player)
{
    player_ = player;

    status_ = xEventGroupCreate();
    if (! status_) {
        ESP_LOGE(TAG, "Unable to crate status bits");
        return false;
    }
    xEventGroupSetBits(status_, STATUS_HM_IDLE);

    if (xTaskCreate(task, "himax_i2c", 4096, this, 5, nullptr) != pdTRUE)
        return false;

    gpio_reset_pin(HIMAX_RESET_PIN);
    gpio_set_direction(HIMAX_RESET_PIN, GPIO_MODE_OUTPUT);
    dev_reset();
    vTaskDelay(pdMS_TO_TICKS(300));
    dev_get_status();

    return true;
}

void HimaxModule::dev_reset()
{
    gpio_set_level(HIMAX_RESET_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(HIMAX_RESET_PIN, 1);
}

uint8_t HimaxModule::dev_get_status()
{
    esp_err_t err;
    transaction_t tr = {.cmd = CMD_GET_STATUS, .data_length = 0};
    i2c_bus_lock(-1);
    err = i2c_master_send((void*)&tr, sizeof(transaction_t), pdMS_TO_TICKS(1000));
    err = i2c_master_recv((void*)&tr, sizeof(transaction_t), pdMS_TO_TICKS(1000));
    i2c_bus_unlock();
    ESP_LOGD(TAG, "status=%s", status_to_str(tr.status));
    return tr.status;
}
