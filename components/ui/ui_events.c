#include "ui_events.h"
#include "ui.h"                 
#include "tts_bridge.h"
#include "builtin_texts.h"
#include "esp_log.h"
#include "lvgl.h"
#include "visuals.h" 

#include "esp_heap_caps.h"  

static const char* TAG_UI = "ui_events";


static void free_all_images_except(const lv_img_dsc_t* except_dsc)
{
    for (int i = 0; i < CASE_TXT_COUNT; ++i) {
        const lv_img_dsc_t* d = kVisuals[i].img;
        if (!d || d == except_dsc) continue;
        if (is_pinned_image(d)) continue;        

        if (d->data) {
            ESP_LOGD(TAG_UI, "free image buffer: %p (case %d)", d->data, i);
            heap_caps_free((void*)d->data);
            ((lv_img_dsc_t*)d)->data = NULL;
            ((lv_img_dsc_t*)d)->data_size = 0;
        }
    }
}

void apply_image_for_case(builtin_text_case_t c)
{
    if (!ui_Img) return;
    if (c < 0 || c >= CASE_TXT_COUNT) return;

    const void* cur_src = lv_img_get_src(ui_Img);
    const lv_img_dsc_t* cur_dsc = NULL;
    if (cur_src && lv_img_src_get_type(cur_src) == LV_IMG_SRC_VARIABLE) {
        cur_dsc = (const lv_img_dsc_t*)cur_src;
    }

    free_all_images_except(cur_dsc);

    const case_visual_t v = kVisuals[c];
    if (v.load) v.load();
    if (v.img)  lv_img_set_src(ui_Img, v.img);

    if (cur_dsc && cur_dsc != v.img && cur_dsc->data) {
        if (!is_pinned_image(cur_dsc)) {         
            ESP_LOGD(TAG_UI, "free previous image buffer: %p", cur_dsc->data);
            heap_caps_free((void*)cur_dsc->data);
            ((lv_img_dsc_t*)cur_dsc)->data = NULL;
            ((lv_img_dsc_t*)cur_dsc)->data_size = 0;
        }
    }
}

void on_btn_change_pressed(lv_event_t * e)
{
    (void)e;
    builtin_text_next();
    builtin_text_case_t c = builtin_text_get();
    ESP_LOGI(TAG_UI, "Built-in text changed to case #%d", (int)c);
    apply_image_for_case(c);
}

void on_btn_say_pressed(lv_event_t * e)
{
    (void)e;
    const char* text = get_builtin_text();
    // lv_obj_add_state(ui_btnsay, LV_STATE_DISABLED);  
    start_tts_playback_c(text);
}

static void ui_notify_tts_finished_async(void *arg)
{
    (void)arg;
    // lv_obj_clear_state(ui_btnsay, LV_STATE_DISABLED);
}

void ui_notify_tts_finished(void)
{
    lv_async_call(ui_notify_tts_finished_async, NULL);
}

void button_choose_1(lv_event_t * e)
{
	// Your code here
}

void button_choose_2(lv_event_t * e)
{
	// Your code here
}

void button_choose_end(lv_event_t * e)
{
	// Your code here
}