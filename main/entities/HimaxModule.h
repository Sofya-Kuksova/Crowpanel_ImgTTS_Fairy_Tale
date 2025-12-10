#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdlib.h>

#include "entities/AudioPlayer.h"
#include "i2c_comm/i2c_protocol.h"

class HimaxModule
{
public:
    bool init(AudioPlayer* player);
    int sendText(const char* str, size_t xTicksToWait);

private:
    void dev_reset();
    uint8_t dev_get_status();
    static void task(void*);

private:
    static constexpr size_t kPlayerStartFramesThreshold = 10;
    static constexpr char TAG[]                         = "HimaxModule";
    EventGroupHandle_t status_;
    AudioPlayer* player_;
};
