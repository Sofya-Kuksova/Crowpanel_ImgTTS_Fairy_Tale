#include <lvgl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "AudioPlayer.h"
#include "i2s/i2s_tx.h"

#define SPK_MUTE_PIN GPIO_NUM_3
#define SPK_SHUT_PIN GPIO_NUM_4

#define STATUS_PLAYING BIT0

void AudioPlayer::task(void* arg)
{
    AudioPlayer* player = static_cast<AudioPlayer*>(arg);
    uint8_t* buffer     = (uint8_t*)malloc(player->frame_size_);
    size_t read         = 0;
    for (;;) {
        if (xEventGroupWaitBits(player->status_, STATUS_PLAYING, pdFALSE, pdFALSE, portMAX_DELAY) & STATUS_PLAYING) {
            read = fifo_ringbuf_read(player->audio_buffer_, buffer, player->frame_size_, portMAX_DELAY);
            if (read) {
                ESP_LOGV(TAG, "RB, pop, %u", fifo_ringbuf_size(player->audio_buffer_));
                i2s_tx_write(buffer, read, portMAX_DELAY);
            }
        }
    }
    free(buffer);
}

bool AudioPlayer::init(esp_expander::Base* io_expander, size_t frame_size_)
{
    this->io_expander_ = io_expander;

    this->frame_size_ = frame_size_;
    audio_buffer_     = fifo_ringbuf_init(kRingBufferSize, frame_size_);
    if (! audio_buffer_) {
        return false;
    }

    i2s_tx_init();

    io_expander_->pinMode(SPK_MUTE_PIN, OUTPUT);
    io_expander_->pinMode(SPK_SHUT_PIN, OUTPUT);
    io_expander_->digitalWrite(SPK_MUTE_PIN, HIGH);
    io_expander_->digitalWrite(SPK_SHUT_PIN, LOW);

    ESP_LOGD(TAG, "MUTE is %d", io_expander_->digitalRead(SPK_MUTE_PIN));
    ESP_LOGD(TAG, "SHUTDOWN is %d", io_expander_->digitalRead(SPK_SHUT_PIN));

    status_ = xEventGroupCreate();
    if (! status_) {
        ESP_LOGE(TAG, "Unable to crate status bits");
        return false;
    }
    xEventGroupClearBits(status_, STATUS_PLAYING);

    if (xTaskCreate(task, "AudioPlayer", 4096, this, 2, nullptr) != pdTRUE)
        return false;

    return true;
}

bool AudioPlayer::write(const uint8_t* data, size_t bytes)
{
    ESP_LOGV(TAG, "RB, push, %u", fifo_ringbuf_size(audio_buffer_));
    return fifo_ringbuf_write(audio_buffer_, data, bytes) == bytes;
}

bool AudioPlayer::control(uint8_t value)
{
    if (value) {
        if (xEventGroupGetBits(status_) & STATUS_PLAYING) {
            return false;
        }
        io_expander_->digitalWrite(SPK_MUTE_PIN, LOW);
        xEventGroupSetBits(status_, STATUS_PLAYING);
        ESP_LOGD(TAG, "Start playback");
    } else {
        ESP_LOGV(TAG, "try to stop, %u", fifo_ringbuf_size(audio_buffer_));
        while (! fifo_ringbuf_empty(audio_buffer_)) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        xEventGroupClearBits(status_, STATUS_PLAYING);
        io_expander_->digitalWrite(SPK_MUTE_PIN, HIGH);
        ESP_LOGD(TAG, "Stop playback");
    }
    return true;
}

bool AudioPlayer::isOn()
{
    return xEventGroupGetBits(status_) & STATUS_PLAYING;
}

float AudioPlayer::getBufferUtilization()
{
    return static_cast<float>(fifo_ringbuf_size(audio_buffer_)) / kRingBufferSize;
}
