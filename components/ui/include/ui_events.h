#pragma once
#include "lvgl.h"
#include "builtin_texts.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Управление картинками сказки (small/large) */
void apply_image_for_case(builtin_text_case_t c);

/* Debug-кнопки (если используются где-то в интерфейсе) */
void on_btn_change_pressed(lv_event_t * e);
void on_btn_say_pressed(lv_event_t * e);

/* Обновление текста по UART (пока можно не использовать) */
void on_text_update_from_uart(const char *text);

/* Колбэк от TTS-модуля: озвучка текущего текста закончилась */
void ui_notify_tts_finished(void);

/* Выбор вариантов истории (кнопки ui_ch1 / ui_ch2) */
void button_choose_1(lv_event_t * e);
void button_choose_2(lv_event_t * e);

/* Старт истории (первый кейс) */
void ui_story_start(void);

/* Служебные обработчики, сгенерированные SquareLine */
void change_screen_to_current(lv_event_t * e);
void reload_app(lv_event_t * e);
void save_name(lv_event_t * e);

/* Финальная кнопка "Try again" (ui_end2) */
void end_event(lv_event_t * e);

void ui_handle_settings_from_screen1(lv_event_t * e);
void ui_handle_settings_from_screen2(lv_event_t * e);
void ui_handle_settings_back_from_screen3(lv_event_t * e);

void ui_handle_choice1(lv_event_t * e);
void ui_handle_choice2(lv_event_t * e);

void reload_app(lv_event_t * e);

#ifdef __cplusplus
}
#endif
