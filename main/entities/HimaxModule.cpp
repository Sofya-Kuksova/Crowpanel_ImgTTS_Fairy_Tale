// main/entities/HimaxModule.cpp

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

#define STATUS_HM_BUSY   BIT0
#define STATUS_HM_PAUSED BIT1

extern bool i2c_bus_lock(int timeout_ms);
extern bool i2c_bus_unlock(void);

// UI notifications (safe from non-LVGL task: it must hop into LVGL via lv_async_call inside)
extern "C" void ui_notify_tts_finished(void);

void HimaxModule::task(void* arg)
{
    HimaxModule* himax_module = static_cast<HimaxModule*>(arg);
    esp_err_t err;
    const TickType_t xDelay = pdMS_TO_TICKS(14); // audio frame is 16ms, i2c tx is around 12ms

    while (1) {
        if (xEventGroupWaitBits(himax_module->status_, STATUS_HM_BUSY, pdFALSE, pdFALSE, portMAX_DELAY) &
            STATUS_HM_BUSY) {
            size_t packet_counter            = 0;
            size_t corrupted_packets_counter = 0;
            bool stop                        = false;
            uint8_t status                   = 0xff;

            // NEW: protect I2C bus from infinite error-loop
            size_t i2c_timeout_streak      = 0;
            const size_t kMaxI2cTimeouts   = 6;
            const TickType_t kI2cErrBackoff = pdMS_TO_TICKS(30);
            bool aborted                    = false;

            ESP_LOGI(TAG, "Start audio recv");

            TickType_t xLastWakeTime = xTaskGetTickCount();
            while (!stop) {
                if (xEventGroupGetBits(himax_module->status_) & STATUS_HM_PAUSED) {
                    break;
                }

                size_t bursted_packets = 0;

                err = himax_module->dev_get_status(status);
                if (err != ESP_OK) {
                    i2c_timeout_streak++;
                    ESP_LOGE(TAG, "Unable to get dev status (err=%s, streak=%u)",
                             esp_err_to_name(err), (unsigned)i2c_timeout_streak);

                    if (i2c_timeout_streak >= kMaxI2cTimeouts) {
                        aborted = true;
                        stop    = true;
                        break;
                    }

                    vTaskDelay(kI2cErrBackoff);
                    continue;
                }
                i2c_timeout_streak = 0;

                switch (status) {
                case ST_OUTPUT_RDY: {
                    xLastWakeTime = xTaskGetTickCount();
                    while (1) {
                        data_packet_t data;
                        memset(&data, 0, sizeof(data_packet_t));

                        err = himax_module->dev_get_data_packet(data);
                        if (err != ESP_OK) {
                            // will re-check status in outer loop
                            break;
                        }
                        if (data.error != ERR_OK) {
                            break;
                        }

                        uint16_t crc = crc16_compute(crc16_lut, &crc16_ccitt_false_config,
                                                     (const uint8_t*)data.data, data.data_length);

                        ESP_LOGD(TAG, "rx packet: err=%u, idx=%u, total=%u, length=%u, crc=%u(%u)",
                                 data.error, data.index, data.total, data.data_length, data.crc, crc);

                        if (data.crc != crc) {
                            corrupted_packets_counter++;
                            continue;
                        }

                        bursted_packets++;
                        packet_counter++;

                        // if player is off and its buffer is full, will block indefinitely
                        himax_module->player_->write(data.data, data.data_length, portMAX_DELAY);

                        ESP_LOG_BUFFER_HEXDUMP(TAG, &data, sizeof(data_packet_t), ESP_LOG_VERBOSE);
                        vTaskDelayUntil(&xLastWakeTime, xDelay);
                    }
                } break;

                case ST_IDLE:
                    // make sure that dev have started
                    if (packet_counter != 0) {
                        stop = true;
                        break;
                    }
                    break;

                default:
                    break;
                }

                if (bursted_packets > 0) {
                    ESP_LOGI(TAG, "Bursted %u packets", (unsigned)bursted_packets);
                }

                if (packet_counter == 0) {
                    vTaskDelay(pdMS_TO_TICKS(500)); // lower frequency while dev is busy
                } else {
                    vTaskDelayUntil(&xLastWakeTime, xDelay);
                }
            }

            if (packet_counter > 0) {
                ESP_LOGI(TAG, "Recieved %u packets", (unsigned)packet_counter);
            }
            if (corrupted_packets_counter > 0) {
                ESP_LOGW(TAG, "Recieved %u corrupted packets", (unsigned)corrupted_packets_counter);
            }

            // --- FINISH / ABORT decision ---
            const EventBits_t bits_now = xEventGroupGetBits(himax_module->status_);
            const bool paused_now      = (bits_now & STATUS_HM_PAUSED) != 0;

            // “Finished” = we really received audio (packet_counter != 0)
            // and exited by ST_IDLE (stop == true), and it is NOT a pause.
            const bool finished_now = (!paused_now) && (packet_counter != 0) && stop && (!aborted);

            if (finished_now) {
                // Stream ended on Himax side; UI "finished" will be emitted by AudioPlayer
                // when the playback buffer is fully drained.
                if (himax_module->player_) {
                    himax_module->player_->mark_stream_ended();
                }
            }

            if (aborted && !paused_now) {
                ESP_LOGE(TAG, "Abort audio recv due to I2C errors; resetting Himax and stopping player");
                himax_module->dev_reset();
                if (himax_module->player_) {
                    himax_module->player_->stop();
                }
                // Make UI stop TALK (via normal UI path) and proceed safely
                ui_notify_tts_finished();
            }

            xEventGroupClearBits(himax_module->status_, STATUS_HM_BUSY);
        }
    }
}

int HimaxModule::waitReady(size_t timeout)
{
    static constexpr size_t polling_period = pdMS_TO_TICKS(500);
    uint8_t status                         = 0xff;
    size_t elapsed                         = 0;
    do {
        esp_err_t ret = dev_get_status(status);
        if (ret != ESP_OK) {
            return ret;
        }
        vTaskDelay(polling_period);
        elapsed += polling_period;
        if (elapsed > timeout) {
            return ESP_ERR_TIMEOUT;
        }
    } while (status != ST_IDLE);
    return 0;
}

int HimaxModule::sendText(const char* str, size_t xTicksToWait)
{
    if (xEventGroupGetBits(status_) & STATUS_HM_BUSY) {
        return -1;
    }

    if (dev_probe() < 0) {
        return -1;
    }

    if (xEventGroupGetBits(status_) & STATUS_HM_PAUSED) {
        stop();
        if (waitReady(xTicksToWait) < 0) {
            return -1;
        }
    }

    esp_err_t err = ESP_OK;
    size_t n      = strlen(str);
    ESP_LOGI(TAG, "send text: len=%u", (unsigned)n);

    transaction_t tr = {.cmd = CMD_RECV_MSG, .data_length = static_cast<uint16_t>(n)};
    i2c_bus_lock(-1);
    err = i2c_master_send((void*)&tr, sizeof(transaction_t), xTicksToWait);
    if (err != ESP_OK) {
        i2c_bus_unlock();
        return -1;
    }
    err = i2c_master_send((void*)str, n, pdMS_TO_TICKS(1000));
    if (err != ESP_OK) {
        i2c_bus_unlock();
        return -1;
    }
    i2c_bus_unlock();

    return 0;
}

int HimaxModule::start()
{
    if (xEventGroupGetBits(status_) & STATUS_HM_BUSY) {
        return -1;
    }

    esp_err_t err    = ESP_OK;
    transaction_t tr = {.cmd = CMD_START, .data_length = 0};
    i2c_bus_lock(-1);
    err = i2c_master_send((void*)&tr, sizeof(transaction_t), pdMS_TO_TICKS(50));
    if (err != ESP_OK) {
        i2c_bus_unlock();
        return -1;
    }
    i2c_bus_unlock();

    xEventGroupSetBits(status_, STATUS_HM_BUSY);
    return 0;
}

int HimaxModule::stop()
{
    if (dev_probe() < 0) {
        return -1;
    }

    esp_err_t err    = ESP_OK;
    transaction_t tr = {.cmd = CMD_STOP, .data_length = 0};
    i2c_bus_lock(-1);
    err = i2c_master_send((void*)&tr, sizeof(transaction_t), pdMS_TO_TICKS(50));
    if (err != ESP_OK) {
        i2c_bus_unlock();
        return -1;
    }
    i2c_bus_unlock();

    xEventGroupClearBits(status_, STATUS_HM_BUSY | STATUS_HM_PAUSED);
    player_->stop();
    return 0;
}

int HimaxModule::pause()
{
    const auto xBits = xEventGroupGetBits(status_);
    if ((xBits & STATUS_HM_PAUSED) || !(xBits & STATUS_HM_BUSY)) {
        return -1;
    }

    if (dev_probe() < 0) {
        return -1;
    }

    esp_err_t err    = ESP_OK;
    transaction_t tr = {.cmd = CMD_PAUSE, .data_length = 0};
    i2c_bus_lock(-1);
    err = i2c_master_send((void*)&tr, sizeof(transaction_t), pdMS_TO_TICKS(50));
    if (err != ESP_OK) {
        i2c_bus_unlock();
        return -1;
    }
    i2c_bus_unlock();

    xEventGroupSetBits(status_, STATUS_HM_PAUSED);
    player_->pause();
    return 0;
}

int HimaxModule::resume()
{
    if (!(xEventGroupGetBits(status_) & STATUS_HM_PAUSED)) {
        return -1;
    }

    if (dev_probe() < 0) {
        return -1;
    }

    esp_err_t err    = ESP_OK;
    transaction_t tr = {.cmd = CMD_RESUME, .data_length = 0};
    i2c_bus_lock(-1);
    err = i2c_master_send((void*)&tr, sizeof(transaction_t), pdMS_TO_TICKS(50));
    if (err != ESP_OK) {
        i2c_bus_unlock();
        return -1;
    }
    i2c_bus_unlock();

    xEventGroupClearBits(status_, STATUS_HM_PAUSED);
    xEventGroupSetBits(status_, STATUS_HM_BUSY);
    player_->resume();
    return 0;
}

bool HimaxModule::init(AudioPlayer* player)
{
    dev_reset_counter_ = 0;
    player_            = player;

    status_ = xEventGroupCreate();
    if (!status_) {
        ESP_LOGE(TAG, "Unable to crate status bits");
        return false;
    }

    if (xTaskCreate(task, "himax_i2c", 4096, this, 1, nullptr) != pdTRUE)
        return false;

    gpio_reset_pin(HIMAX_RESET_PIN);
    gpio_set_direction(HIMAX_RESET_PIN, GPIO_MODE_OUTPUT);
    dev_reset();
    vTaskDelay(pdMS_TO_TICKS(300));

    if (dev_probe() < 0) {
        return false;
    }

    uint8_t status;
    esp_err_t err = dev_get_status(status);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Unable to get dev status");
        return false;
    }

    return true;
}

void HimaxModule::dev_reset()
{
    ESP_LOGI(TAG, "Reset device: %u", dev_reset_counter_++);
    gpio_set_level(HIMAX_RESET_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(HIMAX_RESET_PIN, 1);
}

int HimaxModule::dev_probe()
{
    i2c_bus_lock(-1);
    if (!i2c_master_dev_available()) {
        i2c_bus_unlock();
        ESP_LOGW(TAG, "Himax module is not available on I2C bus");
        return -1;
    }
    i2c_bus_unlock();
    return 0;
}

int HimaxModule::dev_get_status(uint8_t& status)
{
    esp_err_t err    = ESP_OK;
    transaction_t tr = {.cmd = CMD_GET_STATUS, .data_length = 0};

    i2c_bus_lock(-1);

    // CHANGED: 50ms -> 150ms
    err = i2c_master_send((void*)&tr, sizeof(transaction_t), pdMS_TO_TICKS(150));
    if (err != ESP_OK) {
        i2c_bus_unlock();
        ESP_LOGE(TAG, "I2C err=%s", esp_err_to_name(err));
        return err;
    }

    memset(&tr, 0, sizeof(transaction_t));

    // CHANGED: 50ms -> 150ms
    err = i2c_master_recv((void*)&tr, sizeof(transaction_t), pdMS_TO_TICKS(150));
    if (err != ESP_OK) {
        i2c_bus_unlock();
        ESP_LOGE(TAG, "I2C err=%s", esp_err_to_name(err));
        return err;
    }

    i2c_bus_unlock();

    status = tr.status;
    ESP_LOGD(TAG, "status=%s", status_to_str(status));
    return ESP_OK;
}

int HimaxModule::dev_get_data_packet(data_packet_t& data)
{
    esp_err_t err    = ESP_OK;
    transaction_t tr = {.cmd = CMD_SEND_DATA, .data_length = 0};

    i2c_bus_lock(-1);

    // CHANGED: 50ms -> 150ms
    err = i2c_master_send((void*)&tr, sizeof(transaction_t), pdMS_TO_TICKS(150));
    if (err == ESP_ERR_TIMEOUT) {
        i2c_bus_unlock();
        ESP_LOGE(TAG, "I2C (w) err=%s", esp_err_to_name(err));
        return err;
    }

    // CHANGED: 50ms -> 150ms
    err = i2c_master_recv((void*)&data, sizeof(data_packet_t), pdMS_TO_TICKS(150));
    if (err == ESP_ERR_TIMEOUT) {
        i2c_bus_unlock();
        ESP_LOGE(TAG, "I2C (r) err=%s", esp_err_to_name(err));
        return err;
    }

    i2c_bus_unlock();
    return ESP_OK;
}
