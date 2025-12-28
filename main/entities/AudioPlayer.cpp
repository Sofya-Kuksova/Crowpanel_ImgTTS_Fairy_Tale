#include <lvgl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"

#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "AudioPlayer.h"
#include "i2s/i2s_tx.h"

#include "ui_events.h"

#define SPK_MUTE_PIN GPIO_NUM_3
#define SPK_SHUT_PIN GPIO_NUM_4

#define STATUS_ON      BIT0
#define STATUS_PLAYING BIT1
#define STATUS_STARTED BIT2
#define STATUS_STREAM_END BIT3



extern bool i2c_bus_lock(int timeout_ms);
extern bool i2c_bus_unlock(void);

void AudioPlayer::mark_stream_ended()
{
    xEventGroupSetBits(status_, STATUS_STREAM_END);
}

void AudioPlayer::control_task(void* arg)
{
    AudioPlayer* player = static_cast<AudioPlayer*>(arg);
    for (;;) {
        if (! player->isOn()) {
            if (fifo_ringbuf_size(player->audio_buffer_) > kPlaybackStartThre) {
                ESP_LOGD(TAG, "Start playback");
                player->enable();
                xEventGroupClearBits(player->status_, STATUS_STARTED | STATUS_STREAM_END); // новый playback-session
                xEventGroupSetBits(player->status_, STATUS_PLAYING);

            }
        } else {
            while (! fifo_ringbuf_empty(player->audio_buffer_)) {
                vTaskDelay(pdMS_TO_TICKS(100));
            }

            const EventBits_t bits_now = xEventGroupGetBits(player->status_);
            const bool stream_ended    = (bits_now & STATUS_STREAM_END) != 0;

            xEventGroupClearBits(player->status_, STATUS_PLAYING);
            xEventGroupClearBits(player->status_, STATUS_STARTED);
            xEventGroupClearBits(player->status_, STATUS_STREAM_END);

            ESP_LOGD(TAG, "Stop playback");
            player->disable();

            if (stream_ended) {
                ui_notify_tts_finished();
            }


        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static inline bool has_audio_energy_i16(const uint8_t* buf, size_t bytes)
{
    const int16_t* s = (const int16_t*)buf;
    size_t n = bytes / 2;
    const int16_t thr = 250; // порог "не тишина"

    for (size_t i = 0; i < n; i++) {
        int16_t v = s[i];
        if (v > thr || v < -thr) return true;
    }
    return false;
}


void AudioPlayer::play_task(void* arg)
{
    AudioPlayer* player = static_cast<AudioPlayer*>(arg);
    uint8_t* buffer     = (uint8_t*)malloc(player->frame_size_);
    size_t read         = 0;
    for (;;) {
        if (xEventGroupWaitBits(player->status_, STATUS_PLAYING, pdFALSE, pdFALSE, pdMS_TO_TICKS(100)) &
            STATUS_PLAYING) {
            read = fifo_ringbuf_read(player->audio_buffer_, buffer, player->frame_size_, pdMS_TO_TICKS(100));
            if (read) {
                EventBits_t bits = xEventGroupGetBits(player->status_);
                if (!(bits & STATUS_STARTED) && has_audio_energy_i16(buffer, read)) {
                    xEventGroupSetBits(player->status_, STATUS_STARTED);
                    ui_notify_tts_started();   // запускаем UI чуть раньше вывода в I2S
                }

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

    i2c_bus_lock(-1);
    io_expander_->pinMode(SPK_MUTE_PIN, OUTPUT);
    io_expander_->pinMode(SPK_SHUT_PIN, OUTPUT);
    io_expander_->digitalWrite(SPK_MUTE_PIN, HIGH);
    io_expander_->digitalWrite(SPK_SHUT_PIN, LOW);
    i2c_bus_unlock();

    status_ = xEventGroupCreate();
    if (! status_) {
        ESP_LOGE(TAG, "Unable to crate status bits");
        return false;
    }
    xEventGroupClearBits(status_, STATUS_PLAYING);

    if (xTaskCreate(control_task, "PlayerControlTask", 4096, this, 1, nullptr) != pdTRUE)
        return false;

    if (xTaskCreate(play_task, "PlayerPlayTask", 4096, this, 1, nullptr) != pdTRUE)
        return false;

    return true;
}

bool AudioPlayer::write(const uint8_t* data, size_t bytes, size_t ticks_to_wait)
{
    ESP_LOGV(TAG, "RB, push, %u", fifo_ringbuf_size(audio_buffer_));
    return fifo_ringbuf_write(audio_buffer_, data, bytes, ticks_to_wait) == bytes;
}

bool AudioPlayer::enable()
{
    if (xEventGroupGetBits(status_) & STATUS_ON) {
        return false;
    }
    i2c_bus_lock(-1);
    io_expander_->digitalWrite(SPK_MUTE_PIN, LOW);
    i2c_bus_unlock();
    xEventGroupSetBits(status_, STATUS_ON);
    return true;
}

bool AudioPlayer::disable()
{
    if (! (xEventGroupGetBits(status_) & STATUS_ON)) {
        return false;
    }
    i2c_bus_lock(-1);
    io_expander_->digitalWrite(SPK_MUTE_PIN, HIGH);
    i2c_bus_unlock();
    xEventGroupClearBits(status_, STATUS_ON);
    return true;
}

bool AudioPlayer::pause()
{
    if (! (xEventGroupGetBits(status_) & STATUS_PLAYING)) {
        return false;
    }
    xEventGroupClearBits(status_, STATUS_PLAYING);
    return true;
}

bool AudioPlayer::resume()
{
    if (xEventGroupGetBits(status_) & STATUS_PLAYING) {
        return false;
    }
    xEventGroupSetBits(status_, STATUS_PLAYING);
    return true;
}


bool AudioPlayer::stop()
{
    pause();
    fifo_ringbuf_reset(audio_buffer_);
    return true;
}

bool AudioPlayer::isOn()
{
    return xEventGroupGetBits(status_) & STATUS_ON;
}
