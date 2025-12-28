// components/ui/ui_img_loading_gif.c

#include "ui.h"
#include "lvgl.h"
#include "gifdec.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

#include <stdbool.h>

extern bool lvgl_port_lock(uint32_t timeout_ms);
extern void lvgl_port_unlock(void);


static const char *TAG = "GIF_LOADING";

// пути к GIF в SPIFFS
#define UI_BIRD_NORM_GIF_PATH  "/spiffs/assets/crow_idle.gif"
#define UI_BIRD_TALK_GIF_PATH  "/spiffs/assets/crow_talk.gif"

#define GIF_IDLE_SPEED_PCT   100u   
#define GIF_TALK_SPEED_PCT   320u  

// -------------------------
// Общий конвертер кадра
// -------------------------

static void gif_convert_to_lv_color(gd_GIF    *gd,
                                    uint8_t   *gif_rgb_buf,
                                    lv_color_t *gif_lv_buf,
                                    uint32_t   gif_px_cnt)
{
    if (!gd || !gif_rgb_buf || !gif_lv_buf) {
        return;
    }

    // gifdec заполняет gif_rgb_buf в формате RGB888 (3 байта на пиксель)
    gd_render_frame(gd, gif_rgb_buf);

    for (uint32_t i = 0; i < gif_px_cnt; i++) {
        uint8_t r = gif_rgb_buf[3 * i + 0];
        uint8_t g = gif_rgb_buf[3 * i + 1];
        uint8_t b = gif_rgb_buf[3 * i + 2];
        gif_lv_buf[i] = lv_color_make(r, g, b);
    }
}

// ============================================================
//  GIF #1: Нормальная птица (idle) для ui_bird1 и ui_bird3
// ============================================================

// Дескриптор для idle-анимации.
// Имя оставляем прежнее (ui_img_loading_gif), чтобы не ломать заголовок ui.h.
lv_img_dsc_t ui_img_loading_gif = {
    .header.always_zero = 0,
    .header.w = 0,
    .header.h = 0,
    .header.cf = LV_IMG_CF_TRUE_COLOR,
    .data = NULL,
    .data_size = 0
};

static gd_GIF     *gd_norm          = NULL;
static lv_timer_t *gif_timer_norm   = NULL;
static uint8_t    *gif_rgb_norm     = NULL;
static lv_color_t *gif_lv_norm      = NULL;
static uint32_t    gif_px_cnt_norm  = 0;

static void gif_norm_update_timer(lv_timer_t *timer)
{
    (void)timer;

    if (!gd_norm) {
        return;
    }

    int r = gd_get_frame(gd_norm);
    if (r < 0) {
        ESP_LOGE(TAG, "gd_get_frame(norm) failed");
        return;
    }
    if (r == 0) {
        // достигли конца GIF — перематываем
        gd_rewind(gd_norm);
        r = gd_get_frame(gd_norm);
        if (r <= 0) {
            ESP_LOGE(TAG, "gd_get_frame(norm) after rewind failed, r=%d", r);
            return;
        }
    }

    gif_convert_to_lv_color(gd_norm, gif_rgb_norm, gif_lv_norm, gif_px_cnt_norm);

    // перерисовать только птиц, которые используют idle-анимацию
    if (ui_bird1) lv_obj_invalidate(ui_bird1);
    if (ui_bird3) lv_obj_invalidate(ui_bird3);

    // задержка в 1/100 секунды
    // задержка в 1/100 секунды, с разумным минимумом
    // задержка в 1/100 секунды, с разумным минимумом
    uint16_t delay_cs = gd_norm->gce.delay;
    if (delay_cs < 3) {
        delay_cs = 3;        // минимум 30 мс
    }
    uint32_t base_ms = (uint32_t)delay_cs * 10U;

    // Применяем коэффициент скорости для idle:
    // 100% = base_ms, 140% скорости → период примерно base_ms * 100 / 140.
    uint32_t period_ms = (base_ms * 100u + GIF_IDLE_SPEED_PCT / 2u) / GIF_IDLE_SPEED_PCT;
    if (period_ms == 0) {
        period_ms = 1;
    }

    lv_timer_set_period(gif_timer_norm, period_ms);


}

static bool gif_norm_load_once(void)
{
    if (gd_norm) {
        // уже загружено
        return true;
    }

    gd_norm = gd_open_gif(UI_BIRD_NORM_GIF_PATH);
    if (!gd_norm) {
        ESP_LOGE(TAG, "gd_open_gif(\"%s\") failed", UI_BIRD_NORM_GIF_PATH);
        return false;
    }

    int r = gd_get_frame(gd_norm);
    if (r <= 0) {
        ESP_LOGE(TAG, "gd_get_frame(norm) first frame failed, r=%d", r);
        gd_close_gif(gd_norm);
        gd_norm = NULL;
        return false;
    }

    gif_px_cnt_norm = (uint32_t)gd_norm->width * (uint32_t)gd_norm->height;

    // выделяем буферы в PSRAM
    gif_rgb_norm = heap_caps_malloc(gif_px_cnt_norm * 3U, MALLOC_CAP_SPIRAM);
    gif_lv_norm  = heap_caps_malloc(gif_px_cnt_norm * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);

    if (!gif_rgb_norm || !gif_lv_norm) {
        ESP_LOGE(TAG, "malloc for GIF(norm) buffers failed");
        if (gif_rgb_norm) heap_caps_free(gif_rgb_norm);
        if (gif_lv_norm)  heap_caps_free(gif_lv_norm);
        gif_rgb_norm = NULL;
        gif_lv_norm  = NULL;
        gd_close_gif(gd_norm);
        gd_norm = NULL;
        gif_px_cnt_norm = 0;
        return false;
    }

    // первый кадр -> буферы
    gif_convert_to_lv_color(gd_norm, gif_rgb_norm, gif_lv_norm, gif_px_cnt_norm);

    // Настраиваем LVGL-дескриптор
    ui_img_loading_gif.header.w   = gd_norm->width;
    ui_img_loading_gif.header.h   = gd_norm->height;
    ui_img_loading_gif.header.cf  = LV_IMG_CF_TRUE_COLOR;
    ui_img_loading_gif.data       = (const uint8_t *)gif_lv_norm;
    ui_img_loading_gif.data_size  = gif_px_cnt_norm * sizeof(lv_color_t);

    return true;
}

static void ui_bird_norm_gif_start(void)
{
    if (!gif_norm_load_once()) {
        return;
    }

    // подвешиваем idle-анимацию к ui_bird1 и ui_bird3
    if (ui_bird1) lv_img_set_src(ui_bird1, &ui_img_loading_gif);
    if (ui_bird3) lv_img_set_src(ui_bird3, &ui_img_loading_gif);

    if (!gif_timer_norm) {
    uint16_t delay_cs = gd_norm->gce.delay;
    if (delay_cs < 3) {
        delay_cs = 3;
    }
    uint32_t base_ms = (uint32_t)delay_cs * 10U;

    uint32_t period_ms = (base_ms * 100u + GIF_IDLE_SPEED_PCT / 2u) / GIF_IDLE_SPEED_PCT;
    if (period_ms == 0) {
        period_ms = 1;
    }

    gif_timer_norm = lv_timer_create(gif_norm_update_timer, period_ms, NULL);
    }



    ESP_LOGI(TAG, "Idle GIF loaded for ui_bird1/ui_bird3");
}

// ============================================================
//  GIF #2: Говорящая птица (talk) для ui_bird2
// ============================================================

static lv_img_dsc_t ui_img_talk_gif = {
    .header.always_zero = 0,
    .header.w = 0,
    .header.h = 0,
    .header.cf = LV_IMG_CF_TRUE_COLOR,
    .data = NULL,
    .data_size = 0
};

static gd_GIF     *gd_talk          = NULL;
static lv_timer_t *gif_timer_talk   = NULL;
static uint8_t    *gif_rgb_talk     = NULL;
static lv_color_t *gif_lv_talk      = NULL;
static uint32_t    gif_px_cnt_talk  = 0;

// Флаги, которыми управляют TTS-таски
static volatile bool s_talk_anim_active = false;   // true, когда TTS говорит
static volatile bool s_talk_need_rewind  = false;  // нужно отрисовать первый кадр


static void gif_talk_update_timer(lv_timer_t *timer)
{
    (void)timer;

    if (!gd_talk) {
        return;
    }

    // 1) Если нужно вернуть первый кадр после остановки TTS
    if (s_talk_need_rewind) {
        gd_rewind(gd_talk);
        int r = gd_get_frame(gd_talk);
        if (r > 0) {
            gif_convert_to_lv_color(gd_talk, gif_rgb_talk, gif_lv_talk, gif_px_cnt_talk);
            if (ui_bird2) {
                lv_obj_invalidate(ui_bird2);
            }
        } else {
            ESP_LOGE(TAG, "gif_talk_update_timer: rewind first frame failed, r=%d", r);
        }
        s_talk_need_rewind = false;

        // Можно сразу пересчитать период, чтобы дальше всё шло ровно
        uint16_t delay_cs = gd_talk->gce.delay;
        if (delay_cs < 3) {
            delay_cs = 3;
        }
        uint32_t base_ms = (uint32_t)delay_cs * 10U;
        uint32_t period_ms = (base_ms * 100u + GIF_TALK_SPEED_PCT / 2u) / GIF_TALK_SPEED_PCT;
        if (period_ms == 0) {
            period_ms = 1;
        }
        lv_timer_set_period(gif_timer_talk, period_ms);

        return; // на этом тик таймера заканчиваем
    }

    // 2) Если TTS сейчас молчит — кадры не двигаем
    if (!s_talk_anim_active) {
        return;
    }

    // 3) Обычное продвижение GIF, когда TTS говорит
    int r = gd_get_frame(gd_talk);
    if (r < 0) {
        ESP_LOGE(TAG, "gd_get_frame(talk) failed");
        return;
    }
    if (r == 0) {
        gd_rewind(gd_talk);
        r = gd_get_frame(gd_talk);
        if (r <= 0) {
            ESP_LOGE(TAG, "gd_get_frame(talk) after rewind failed, r=%d", r);
            return;
        }
    }

    gif_convert_to_lv_color(gd_talk, gif_rgb_talk, gif_lv_talk, gif_px_cnt_talk);

    if (ui_bird2) {
        lv_obj_invalidate(ui_bird2);
    }

    uint16_t delay_cs = gd_talk->gce.delay;
    if (delay_cs < 3) {
        delay_cs = 3;
    }
    uint32_t base_ms = (uint32_t)delay_cs * 10U;

    uint32_t period_ms = (base_ms * 100u + GIF_TALK_SPEED_PCT / 2u) / GIF_TALK_SPEED_PCT;
    if (period_ms == 0) {
        period_ms = 1;
    }

    lv_timer_set_period(gif_timer_talk, period_ms);
}



static bool gif_talk_load_once(void)
{
    if (gd_talk) {
        return true;
    }

    gd_talk = gd_open_gif(UI_BIRD_TALK_GIF_PATH);
    if (!gd_talk) {
        ESP_LOGE(TAG, "gd_open_gif(\"%s\") failed", UI_BIRD_TALK_GIF_PATH);
        return false;
    }

    int r = gd_get_frame(gd_talk);
    if (r <= 0) {
        ESP_LOGE(TAG, "gd_get_frame(talk) first frame failed, r=%d", r);
        gd_close_gif(gd_talk);
        gd_talk = NULL;
        return false;
    }

    gif_px_cnt_talk = (uint32_t)gd_talk->width * (uint32_t)gd_talk->height;

    gif_rgb_talk = heap_caps_malloc(gif_px_cnt_talk * 3U, MALLOC_CAP_SPIRAM);
    gif_lv_talk  = heap_caps_malloc(gif_px_cnt_talk * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);

    if (!gif_rgb_talk || !gif_lv_talk) {
        ESP_LOGE(TAG, "malloc for GIF(talk) buffers failed");
        if (gif_rgb_talk) heap_caps_free(gif_rgb_talk);
        if (gif_lv_talk)  heap_caps_free(gif_lv_talk);
        gif_rgb_talk = NULL;
        gif_lv_talk  = NULL;
        gd_close_gif(gd_talk);
        gd_talk = NULL;
        gif_px_cnt_talk = 0;
        return false;
    }

    gif_convert_to_lv_color(gd_talk, gif_rgb_talk, gif_lv_talk, gif_px_cnt_talk);

    ui_img_talk_gif.header.w   = gd_talk->width;
    ui_img_talk_gif.header.h   = gd_talk->height;
    ui_img_talk_gif.header.cf  = LV_IMG_CF_TRUE_COLOR;
    ui_img_talk_gif.data       = (const uint8_t *)gif_lv_talk;
    ui_img_talk_gif.data_size  = gif_px_cnt_talk * sizeof(lv_color_t);

    return true;
}

static void ui_bird_talk_gif_start(void)
{
    if (!gif_talk_load_once()) {
        return;
    }

    if (ui_bird2) {
        lv_img_set_src(ui_bird2, &ui_img_talk_gif);   // сразу показываем первый кадр
    }

    if (!gif_timer_talk) {
        uint16_t delay_cs = gd_talk->gce.delay;
        if (delay_cs < 3) {
            delay_cs = 3;
        }
        uint32_t base_ms = (uint32_t)delay_cs * 10U;

        uint32_t period_ms = (base_ms * 100u + GIF_TALK_SPEED_PCT / 2u) / GIF_TALK_SPEED_PCT;
        if (period_ms == 0) {
            period_ms = 1;
        }

        gif_timer_talk = lv_timer_create(gif_talk_update_timer, period_ms, NULL);
    }

    // По умолчанию TTS молчит
    s_talk_anim_active = false;
    s_talk_need_rewind = false;

    ESP_LOGI(TAG, "Talk GIF loaded for ui_bird2");
}


// Эти функции вызываются из TTS-тасков

void ui_bird_talk_anim_start(void)
{
    s_talk_anim_active = true;

    // Сразу просим LVGL выполнить тик таймера и перерисоваться,
    // чтобы "движение" началось без ожидания периода кадра.
    if (gif_timer_talk) {
        lv_timer_ready(gif_timer_talk);
    }
    if (ui_bird2) lv_obj_invalidate(ui_bird2);
    if (ui_bird1) lv_obj_invalidate(ui_bird1);
}


void ui_bird_talk_anim_stop(void)
{
    // Перестаём крутить кадры и просим таймер
    // на следующем тике вернуть первый кадр.
    s_talk_anim_active = false;
    s_talk_need_rewind = true;
}

// --- Дополнительные хелперы для Screen1 ---

// Переводим ворону на Screen1 в режим TALK (тот же GIF, что и для ui_bird2)
void ui_bird1_use_talk_gif(void)
{
    if (!gif_talk_load_once()) {
        return;
    }
    if (ui_bird1) {
        lv_img_set_src(ui_bird1, &ui_img_talk_gif);
        lv_obj_invalidate(ui_bird1);
    }
}

// Возвращаем ворону на Screen1 в обычный idle GIF
void ui_bird1_use_idle_gif(void)
{
    if (!gif_norm_load_once()) {
        return;
    }
    if (ui_bird1) {
        lv_img_set_src(ui_bird1, &ui_img_loading_gif);
        lv_obj_invalidate(ui_bird1);
    }
}

// ============================================================
//  Публичный API (ui.h): load/stop
// ============================================================

void ui_img_loading_gif_load(void)
{
    // Поднимаем обе анимации:
    //  - idle для ui_bird1 и ui_bird3
    //  - talk для ui_bird2
    ui_bird_norm_gif_start();
    ui_bird_talk_gif_start();
}

void ui_img_loading_gif_stop(void)
{
    // Останавливаем idle
    if (gif_timer_norm) {
        lv_timer_del(gif_timer_norm);
        gif_timer_norm = NULL;
    }
    if (gd_norm) {
        gd_close_gif(gd_norm);
        gd_norm = NULL;
    }
    if (gif_rgb_norm) {
        heap_caps_free(gif_rgb_norm);
        gif_rgb_norm = NULL;
    }
    if (gif_lv_norm) {
        heap_caps_free(gif_lv_norm);
        gif_lv_norm = NULL;
    }
    gif_px_cnt_norm = 0;

    // Останавливаем talk
    if (gif_timer_talk) {
        lv_timer_del(gif_timer_talk);
        gif_timer_talk = NULL;
    }
    if (gd_talk) {
        gd_close_gif(gd_talk);
        gd_talk = NULL;
    }
    if (gif_rgb_talk) {
        heap_caps_free(gif_rgb_talk);
        gif_rgb_talk = NULL;
    }
    if (gif_lv_talk) {
        heap_caps_free(gif_lv_talk);
        gif_lv_talk = NULL;
    }
    gif_px_cnt_talk = 0;
}
