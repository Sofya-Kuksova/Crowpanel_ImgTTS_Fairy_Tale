#include "tts_bridge.h"
#include "ui.h"   // для ui_bird_talk_anim_start/stop

// Коллбек, который регистрируется из main.cpp
static start_tts_cb_t g_cb = NULL;

// Реальная "стоп"-функция из main.cpp (там ты сделаешь stop_tts_playback_impl)
extern void stop_tts_playback_impl(void);

void start_tts_playback_c(const char *text)
{
    if (!text || !g_cb) {
        return;
    }

    // Любой запуск TTS синхронизируем с TALK-анимацией вороны
    ui_bird_talk_anim_start();

    // Передаём строку в реализацию из main.cpp (через очередь и worker)
    g_cb(text);
}

void register_start_tts_cb(start_tts_cb_t cb)
{
    g_cb = cb;
}

// Новый API: принудительная остановка TTS из UI (кнопка "Try again")
void tts_stop_playback(void)
{
    // Остановить звук на модуле HX6538
    stop_tts_playback_impl();

    // И сразу же остановить TALK-анимацию (в т.ч. вернуть первый кадр)
    ui_bird_talk_anim_stop();
}
