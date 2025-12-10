#pragma once

#include "freertos/event_groups.h"
#include "esp_display_panel.hpp"

#include "fifo_ringbuf.h"

class AudioPlayer
{
public:
    bool init(esp_expander::Base* io_expander, size_t frame_size);
    bool write(const uint8_t* data, size_t bytes);
    bool control(uint8_t value);
    bool isOn();
    float getBufferUtilization();

private:
    static void task(void*);

private:
    static constexpr size_t kRingBufferSize = 200;
    static constexpr char TAG[]             = "AudioPlayer";
    fifo_ringbuf_t* audio_buffer_;
    size_t frame_size_;
    esp_expander::Base* io_expander_;
    EventGroupHandle_t status_;
};
