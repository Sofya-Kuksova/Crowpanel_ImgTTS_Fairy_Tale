#include "tts_bridge.h"
#include "ui.h"   // для ui_bird_talk_anim_start/stop

// Коллбек, который регистрируется из main.cpp
static start_tts_cb_t g_cb = NULL;

// Реальные реализации из main.cpp
extern void stop_tts_playback_impl(void);
extern void pause_tts_playback_impl(void);
extern void resume_tts_playback_impl(void);

void start_tts_playback_c(const char *text)
{
    if (!text || !g_cb) {
        return;
    }

    // NOTE: TALK-GIF запускается по факту начала аудио (см. ui_notify_tts_started)

    // Передаём строку в реализацию из main.cpp (через очередь и worker)
    g_cb(text);
}

void register_start_tts_cb(start_tts_cb_t cb)
{
    g_cb = cb;
}

// Принудительная остановка TTS из UI (кнопка "Try again")
void tts_stop_playback(void)
{
    // Остановить звук на модуле HX6538
    stop_tts_playback_impl();

    // И сразу же остановить TALK-анимацию (в т.ч. вернуть первый кадр)
    ui_bird_talk_anim_stop();
}

/* --- Новое: пауза/продолжение TTS для экрана настроек --- */

void tts_pause_playback(void)
{
    // Пауза на модуле HX6538 (без завершения кейса)
    pause_tts_playback_impl();

    // Остановить TALK-GIF (можно просто вернуть птицу в idle-состояние)
    ui_bird_talk_anim_stop();
}

void tts_resume_playback(void)
{
    resume_tts_playback_impl();

    // NOTE: TALK-GIF запускается по факту начала аудио (см. ui_notify_tts_started)
}