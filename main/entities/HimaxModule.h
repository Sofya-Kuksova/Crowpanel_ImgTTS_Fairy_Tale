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
    int  sendText(const char* str, size_t xTicksToWait);

    // --- Новое: управление TTS на уровне модуля ---
    void requestAbort();   // аналог старого STOP
    void requestPause();   // пауза (перестаём читать аудио)
    void requestResume();  // продолжить после паузы
    bool isPaused() const;

private:
    void    dev_reset();
    uint8_t dev_get_status();
    static void task(void*);

private:
    static constexpr size_t kPlayerStartFramesThreshold = 10;
    static constexpr char   TAG[]                       = "HimaxModule";

    EventGroupHandle_t status_ = nullptr;
    AudioPlayer*       player_ = nullptr;

    volatile bool abort_requested_ = false;
    volatile bool paused_          = false;
};
