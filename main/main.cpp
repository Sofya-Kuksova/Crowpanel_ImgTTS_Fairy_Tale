#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_spiffs.h"

#include "esp_display_panel.hpp"
#include "lvgl_v8_port.h"

#include "ui.h"
#include "tts_bridge.h"

#include "uart_manager.h"
#include "uart_json.h"
#include "ui_events.h"
#include "user_name_store.h"

// --- Новые заголовки из add-on ---
#include "entities/AudioPlayer.h"
#include "entities/HimaxModule.h"
#include "entities/i2c_comm/i2c_master.h"
#include "entities/i2c_comm/i2c_protocol.h"

using namespace esp_panel::drivers;
using namespace esp_panel::board;

static const char* TAG = "main";

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_WARN

// --- Новый стек TTS + аудио ---

static AudioPlayer   s_audio_player;
static HimaxModule   s_himax_module;

// Мьютекс для общей I2C-шины (как в эталоне)
SemaphoreHandle_t    i2c_mtx = nullptr;

// Очередь и таск для TTS
static QueueHandle_t s_tts_queue = nullptr;
static TaskHandle_t  s_tts_task  = nullptr;

static void tts_worker_task(void* arg)
{
    (void)arg;
    const char* text = nullptr;

    ESP_LOGI(TAG, "TTS worker task started");

    for (;;) {
        if (xQueueReceive(s_tts_queue, &text, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (!text) {
            continue;
        }

        ESP_LOGD(TAG, "TTS worker: speak text @%p", text);

        // Сообщаем UI, что ворона начала говорить → запустить GIF_TALK
        ui_bird_talk_anim_start();

        // Теперь текст уходит в Himax add-on по I²C.
        // Внутри HimaxModule:
        //   - опрашивается статус,
        //   - текст кладётся в буфер,
        //   - модуль начинает генерировать PCM, который через AudioPlayer → I2S → динамик.
        if (s_himax_module.sendText(text, pdMS_TO_TICKS(1000)) != 0) {
            ESP_LOGE(TAG, "HimaxModule::sendText failed");
        }
    }
}

static bool fs_ready_cb(lv_fs_drv_t*) { return true; }

static void* fs_open_cb(lv_fs_drv_t*, const char* path, lv_fs_mode_t mode) {
    
    char real[256];
    snprintf(real, sizeof real, "/spiffs/%s", path);

    const char* m = (mode & LV_FS_MODE_WR) ? ((mode & LV_FS_MODE_RD) ? "rb+" : "wb")
                                           : "rb";
    ESP_LOGI("FS","fopen: %s", real);
    FILE* fp = fopen(real, m);
    if(!fp) ESP_LOGE("FS","fopen failed");
    return fp; 
}

static lv_fs_res_t fs_close_cb(lv_fs_drv_t*, void* f) {
    return f && fclose((FILE*)f) == 0 ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}
static lv_fs_res_t fs_read_cb (lv_fs_drv_t*, void* f, void* buf, uint32_t btr, uint32_t* br){
    if(!f) return LV_FS_RES_FS_ERR;
    size_t n = fread(buf,1,btr,(FILE*)f);
    if(br) *br = (uint32_t)n;
    return ferror((FILE*)f) ? LV_FS_RES_FS_ERR : LV_FS_RES_OK;
}
static lv_fs_res_t fs_seek_cb (lv_fs_drv_t*, void* f, uint32_t pos, lv_fs_whence_t w){
    if(!f) return LV_FS_RES_FS_ERR;
    int wh = (w==LV_FS_SEEK_SET)?SEEK_SET:(w==LV_FS_SEEK_CUR)?SEEK_CUR:SEEK_END;
    return fseek((FILE*)f, (long)pos, wh)==0? LV_FS_RES_OK: LV_FS_RES_FS_ERR;
}
static lv_fs_res_t fs_tell_cb (lv_fs_drv_t*, void* f, uint32_t* pos){
    if(!f) return LV_FS_RES_FS_ERR;
    long p = ftell((FILE*)f);
    if(p < 0) return LV_FS_RES_FS_ERR;
    *pos = (uint32_t)p;
    return LV_FS_RES_OK;
}

void lvgl_register_drive_S(void) {
    lv_fs_drv_t d; lv_fs_drv_init(&d);
    d.letter   = 'S';
    d.ready_cb = fs_ready_cb;
    d.open_cb  = fs_open_cb;
    d.close_cb = fs_close_cb;
    d.read_cb  = fs_read_cb;
    d.seek_cb  = fs_seek_cb;
    d.tell_cb  = fs_tell_cb;
    lv_fs_drv_register(&d);

    char letters[16]={0}; lv_fs_get_letters(letters);
    ESP_LOGI("LVFS","letters: %s", letters);   
}

extern "C" void start_tts_playback_impl(const char* text)
{
    if (!text) {
        return;
    }
    if (!s_tts_queue) {
        ESP_LOGE(TAG, "start_tts_playback_impl: TTS queue not initialized");
        return;
    }

    const char* msg = text;

    if (xQueueSend(s_tts_queue, &msg, 0) != pdPASS) {
        ESP_LOGW(TAG, "start_tts_playback_impl: TTS queue full, drop request");
    }
}


extern "C" void stop_tts_playback_impl(void)
{
    ESP_LOGI(TAG, "stop_tts_playback_impl: abort Himax TTS and stop audio");

    // Аналог старого HM_DEV_CMD_STOP:
    //  - говорим HimaxModule сбросить устройство
    //  - останавливаем вывод звука и очищаем буфер
    s_himax_module.requestAbort();
    s_audio_player.control(0);

    // Ворона перестаёт говорить визуально
    ui_bird_talk_anim_stop();
    // НЕ вызываем ui_notify_tts_finished() — как и раньше при стопе
}

extern "C" void pause_tts_playback_impl(void)
{
    ESP_LOGI(TAG, "pause_tts_playback_impl: pause Himax and mute audio");

    // Логическая пауза:
    //  - HimaxModule перестаёт читать данные с модуля
    //  - звук глушится, буфер остаётся
    s_himax_module.requestPause();
    s_audio_player.mute();
}

extern "C" void resume_tts_playback_impl(void)
{
    ESP_LOGI(TAG, "resume_tts_playback_impl: resume Himax and unmute audio");

    // Продолжаем:
    //  - снова читаем данные с модуля
    //  - возвращаем звук
    s_himax_module.requestResume();
    s_audio_player.unmute();
}



bool i2c_bus_lock(int timeout_ms)
{
    if (!i2c_mtx) {
        ESP_LOGE(TAG, "i2c mutex is not initialized");
        return false;
    }
    const TickType_t timeout_ticks =
        (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return (xSemaphoreTake(i2c_mtx, timeout_ticks) == pdTRUE);
}

bool i2c_bus_unlock(void)
{
    if (!i2c_mtx) {
        ESP_LOGE(TAG, "i2c mutex is not initialized");
        return false;
    }
    xSemaphoreGive(i2c_mtx);
    return true;
}

extern "C" void app_main()
{
    // --- SPIFFS как было ---
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "spiffs",
        .max_files = 12,
        .format_if_mount_failed = false
    };
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));

    // --- I2C для Himax add-on (как в эталоне) ---
    i2c_master_init();

    i2c_mtx = xSemaphoreCreateMutex();
    if (!i2c_mtx) {
        ESP_LOGE(TAG, "Create i2c mutex failed");
        return;
    }

    // --- Инициализация платы (LCD, touch, IO-expander) ---
    Board* board = new Board();
    ESP_UTILS_CHECK_FALSE_EXIT(board->init(),  "Board init failed");
    ESP_UTILS_CHECK_FALSE_EXIT(board->begin(), "Board begin failed");

    // --- Аудио-тракт: AudioPlayer + HimaxModule ---
    auto* io_expander = board->getIO_Expander()->getBase();
    s_audio_player.init(io_expander, PAYLOAD_SIZE);
    s_himax_module.init(&s_audio_player);

    // --- LVGL + UI ---
    ESP_UTILS_CHECK_FALSE_EXIT(
        lvgl_port_init(board->getLCD(), board->getTouch()),
        "LVGL init failed"
    );

    lvgl_register_drive_S();
    ui_init();
    ESP_UTILS_CHECK_FALSE_EXIT(lvgl_port_start(), "LVGL start failed");

    // --- Очередь и worker для TTS ---
    s_tts_queue = xQueueCreate(4, sizeof(const char*));
    if (!s_tts_queue) {
        ESP_LOGE(TAG, "Failed to create TTS queue");
        return;
    }

    BaseType_t r = xTaskCreatePinnedToCore(
        tts_worker_task,
        "tts_worker",
        4096,
        nullptr,
        tskIDLE_PRIORITY,   // пусть останется минимальным
        &s_tts_task,
        0          
    );
    if (r != pdPASS) {
        ESP_LOGE(TAG, "Failed to create TTS worker task");
        return;
    }

    // Регистрируем коллбек для UI → TTS
    register_start_tts_cb(start_tts_playback_impl);

    // UART JSON оставляем как был
    uart_json_init(on_text_update_from_uart);

    // checkStatus/checkError для HxTTS больше не нужны — протокол другой.
}
