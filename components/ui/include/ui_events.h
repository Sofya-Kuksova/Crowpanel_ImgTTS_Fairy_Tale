#pragma once
#include "lvgl.h"
#include "builtin_texts.h"  

#ifdef __cplusplus
extern "C" {
#endif

void apply_image_for_case(builtin_text_case_t c);

void on_btn_change_pressed(lv_event_t * e);
void on_btn_say_pressed(lv_event_t * e);
void on_text_update_from_uart(const char *text);
void ui_notify_tts_finished(void);

void button_choose_1(lv_event_t * e);
void button_choose_2(lv_event_t * e);
void button_choose_end(lv_event_t * e);

#ifdef __cplusplus
}
#endif
