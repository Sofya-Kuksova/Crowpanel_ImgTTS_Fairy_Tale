// components/ui/ui_img_loading_gif.c

#include "ui.h"
#include "lvgl.h"
#include "gifdec.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "GIF_LOADING";

// путь к GIF в SPIFFS
#define UI_LOADING_GIF_PATH  "/spiffs/assets/bird_wh.gif"

// LVGL-дескриптор изображения для ui_GifLoading
lv_img_dsc_t ui_img_loading_gif = {
    .header.always_zero = 0,
    .header.w = 0,
    .header.h = 0,
    .header.cf = LV_IMG_CF_TRUE_COLOR,   // буфер будет в формате lv_color_t
    .data = NULL,
    .data_size = 0
};

static gd_GIF     *gd          = NULL;
static lv_timer_t *gif_timer   = NULL;

// буфер: RGB888 от gifdec
static uint8_t    *gif_rgb_buf = NULL;
// буфер: в формате lv_color_t (RGB565 при LV_COLOR_DEPTH=16)
static lv_color_t *gif_lv_buf  = NULL;
static uint32_t    gif_px_cnt  = 0;

// конвертация одного кадра из GIF -> LVGL-буфер
static void gif_convert_to_lv_color(void)
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

// таймер обновления кадра GIF
static void gif_update_timer(lv_timer_t *timer)
{
    if (!gd) {
        return;
    }

    int r = gd_get_frame(gd);
    if (r < 0) {
        ESP_LOGE(TAG, "gd_get_frame() failed");
        return;
    }
    if (r == 0) {
        // достигли трейлера GIF — перематываем и берём первый кадр
        gd_rewind(gd);
        r = gd_get_frame(gd);
        if (r <= 0) {
            ESP_LOGE(TAG, "gd_get_frame() after rewind failed, r=%d", r);
            return;
        }
    }

    // сконвертировать текущий кадр в lv_color_t
    gif_convert_to_lv_color();

    lv_obj_t *img = (lv_obj_t *)timer->user_data;
    if (img) {
        lv_obj_invalidate(img); // перерисовать объект
    }

    // задержка в 1/100 секунды
    uint16_t delay_cs  = gd->gce.delay;
    uint32_t period_ms = delay_cs ? (uint32_t)delay_cs * 10U : 100U;
    lv_timer_set_period(timer, period_ms);
}

void ui_img_loading_gif_load(void)
{
    if (gd) {
        // уже загружено
        return;
    }

    gd = gd_open_gif(UI_LOADING_GIF_PATH);
    if (!gd) {
        ESP_LOGE(TAG, "gd_open_gif(\"%s\") failed", UI_LOADING_GIF_PATH);
        return;
    }

    int r = gd_get_frame(gd);
    if (r <= 0) {
        ESP_LOGE(TAG, "gd_get_frame() first frame failed, r=%d", r);
        gd_close_gif(gd);
        gd = NULL;
        return;
    }

    gif_px_cnt = (uint32_t)gd->width * (uint32_t)gd->height;

    // выделяем буферы (маленький GIF — это дешево)
    gif_rgb_buf = heap_caps_malloc(gif_px_cnt * 3U, MALLOC_CAP_SPIRAM);
    gif_lv_buf  = heap_caps_malloc(gif_px_cnt * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);

    if (!gif_rgb_buf || !gif_lv_buf) {
        ESP_LOGE(TAG, "malloc for GIF buffers failed");
        if (gif_rgb_buf) free(gif_rgb_buf);
        if (gif_lv_buf)  free(gif_lv_buf);
        gif_rgb_buf = NULL;
        gif_lv_buf  = NULL;
        gd_close_gif(gd);
        gd = NULL;
        return;
    }

    // первый кадр -> буферы
    gif_convert_to_lv_color();

    // Настраиваем LVGL-дескриптор
    ui_img_loading_gif.header.w = gd->width;
    ui_img_loading_gif.header.h = gd->height;
    ui_img_loading_gif.header.cf = LV_IMG_CF_TRUE_COLOR;
    ui_img_loading_gif.data     = (const uint8_t *)gif_lv_buf;
    ui_img_loading_gif.data_size = gif_px_cnt * sizeof(lv_color_t);

    if (!ui_GifLoading) {
        ESP_LOGW(TAG, "ui_GifLoading is NULL (screen not created yet?)");
    } else {
        // Назначаем источник картинки
        lv_img_set_src(ui_GifLoading, &ui_img_loading_gif);

        // запускаем таймер анимации
        uint16_t delay_cs  = gd->gce.delay;
        uint32_t period_ms = delay_cs ? (uint32_t)delay_cs * 10U : 100U;
        gif_timer = lv_timer_create(gif_update_timer, period_ms, ui_GifLoading);
    }

    ESP_LOGI(TAG, "GIF %dx%d loaded and playing", gd->width, gd->height);
}

void ui_img_loading_gif_stop(void)
{
    if (gif_timer) {
        lv_timer_del(gif_timer);
        gif_timer = NULL;
    }
    if (gd) {
        gd_close_gif(gd);
        gd = NULL;
    }
    if (gif_rgb_buf) {
        free(gif_rgb_buf);
        gif_rgb_buf = NULL;
    }
    if (gif_lv_buf) {
        free(gif_lv_buf);
        gif_lv_buf = NULL;
    }
    gif_px_cnt = 0;
}
