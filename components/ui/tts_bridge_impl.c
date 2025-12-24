// components/ui/tts_bridge_impl.c

#include "tts_bridge.h"
#include "ui.h"     // ui_bird_talk_anim_start/stop
#include "lvgl.h"   // lv_timer_*
#include <stdlib.h>
#include <string.h>

static start_tts_cb_t g_cb = NULL;

// Реальные реализации из main.cpp
extern void stop_tts_playback_impl(void);
extern void pause_tts_playback_impl(void);
extern void resume_tts_playback_impl(void);

#define TTS_START_DELAY_MS  100

// Отложенный запуск
static lv_timer_t *s_tts_delay_timer = NULL;

// Буфер, который ждёт таймер (ещё не отправили в main)
static char *s_pending_text = NULL;

// Буфер, который уже передали в main (держим, чтобы указатель был валиден)
static char *s_inflight_text = NULL;

static void free_pending(void)
{
    if (s_pending_text) {
        free(s_pending_text);
        s_pending_text = NULL;
    }
}

static void free_inflight(void)
{
    if (s_inflight_text) {
        free(s_inflight_text);
        s_inflight_text = NULL;
    }
}

static void cancel_delayed_start(void)
{
    if (s_tts_delay_timer) {
        lv_timer_del(s_tts_delay_timer);
        s_tts_delay_timer = NULL;
    }
    free_pending();
}

static void tts_delay_cb(lv_timer_t *t)
{
    (void)t;

    // one-shot
    if (s_tts_delay_timer) {
        lv_timer_del(s_tts_delay_timer);
        s_tts_delay_timer = NULL;
    }

    if (!g_cb || !s_pending_text) {
        free_pending();
        return;
    }

    // если вдруг прошлый inflight ещё висит — освободим (уже точно не нужен)
    free_inflight();

    // передаём владение "inflight" (НЕ фришим сразу)
    s_inflight_text = s_pending_text;
    s_pending_text  = NULL;

    // Запуск реального TTS (через очередь/worker в main.cpp)
    g_cb(s_inflight_text);
}

void register_start_tts_cb(start_tts_cb_t cb)
{
    g_cb = cb;
}

void start_tts_playback_c(const char *text)
{
    if (!text || !g_cb) {
        return;
    }

    // отменяем предыдущий отложенный старт (если был)
    cancel_delayed_start();

    // запускаем TALK сразу (ради синхры)
    ui_bird_talk_anim_start();

    // готовим копию текста для таймера
    size_t n = strlen(text);
    s_pending_text = (char *)malloc(n + 1);
    if (!s_pending_text) {
        // fallback: если malloc не удался — шлём сразу (без задержки)
        // и держим inflight копию, чтобы указатель был валиден
        free_inflight();
        s_inflight_text = (char *)malloc(n + 1);
        if (s_inflight_text) {
            memcpy(s_inflight_text, text, n + 1);
            g_cb(s_inflight_text);
        } else {
            // крайний случай: шлём как есть
            g_cb(text);
        }
        return;
    }
    memcpy(s_pending_text, text, n + 1);

    // планируем запуск через 100 мс
    s_tts_delay_timer = lv_timer_create(tts_delay_cb, TTS_START_DELAY_MS, NULL);
}

void tts_stop_playback(void)
{
    // если старт ещё не случился — отменяем его
    cancel_delayed_start();

    // останавливаем модуль
    stop_tts_playback_impl();

    // стопаем анимацию
    ui_bird_talk_anim_stop();

    // inflight больше не нужен
    free_inflight();
}

void tts_pause_playback(void)
{
    // если старт ещё не случился — отменяем
    cancel_delayed_start();

    pause_tts_playback_impl();
    ui_bird_talk_anim_stop();

    // inflight держим: при resume модуль продолжит, строка уже не нужна,
    // но освобождать безопасно можно. Если хочешь — можно оставить.
    free_inflight();
}

void tts_resume_playback(void)
{
    resume_tts_playback_impl();
    ui_bird_talk_anim_start();
}
