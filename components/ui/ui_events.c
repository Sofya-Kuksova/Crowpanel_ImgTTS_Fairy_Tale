#include "ui_events.h"
#include "ui.h"
#include "tts_bridge.h"
#include "builtin_texts.h"
#include "esp_log.h"
#include "lvgl.h"
#include "visuals.h"
#include "esp_heap_caps.h"
#include "story_fairy_tale.h"

static builtin_text_case_t s_current = STORY_START_CASE;
static lv_timer_t* s_tts_timer = NULL;
static const char* TAG_UI = "ui_events";

// ---- ПРОТОТИПЫ СТАТИЧЕСКИХ ФУНКЦИЙ (ВАЖНО!)
static void update_question_and_choices(const story_node_t* node);
static void show_case(builtin_text_case_t c);
static void tts_timer_cb(lv_timer_t* t);
static void schedule_tts_after_delay(void);
static void ui_notify_tts_finished_async(void *arg);

// ---- Картинки/память
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

// ---- Кнопки "служебные"
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
    start_tts_playback_c(text);
}

// ---- Событие "TTS закончил"
static void ui_notify_tts_finished_async(void *arg)
{
    (void)arg;
    const story_node_t* node = story_get_node(s_current);
    update_question_and_choices(node);
}

void ui_notify_tts_finished(void)
{
    lv_async_call(ui_notify_tts_finished_async, NULL);
}

// ---- Обработчики выбора
void button_choose_1(lv_event_t * e)
{
    (void)e;
    const story_node_t* node = story_get_node(s_current);
    if (!node || node->is_final) return;
    show_case(node->next1);
}

void button_choose_2(lv_event_t * e)
{
    (void)e;
    const story_node_t* node = story_get_node(s_current);
    if (!node || node->is_final) return;
    show_case(node->next2);
}

void button_choose_end(lv_event_t * e)
{
    (void)e;
    // опционально: перезапуск истории
    // show_case(STORY_START_CASE);
}

// ---- Тексты вопроса/вариантов (показываем ТОЛЬКО после TTS)
static void update_question_and_choices(const story_node_t* node)
{
    if (!ui_LabelQ || !ui_LabelCh1 || !ui_LabelCh2) return;
    if (!node || node->is_final) {
        lv_label_set_text(ui_LabelQ,  "");
        lv_label_set_text(ui_LabelCh1,"");
        lv_label_set_text(ui_LabelCh2,"");
           lv_obj_clear_flag(ui_choice1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_choice2, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_label_set_text(ui_LabelQ,  node->question ? node->question : "");
    lv_label_set_text(ui_LabelCh1,node->choice1  ? node->choice1  : "");
    lv_label_set_text(ui_LabelCh2,node->choice2  ? node->choice2  : "");
        lv_obj_clear_flag(ui_choice1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_choice2, LV_OBJ_FLAG_HIDDEN);
}

// ---- Отложенный запуск TTS (1 сек после смены картинки)
static void tts_timer_cb(lv_timer_t* t)
{
    (void)t;
    const char* text = get_builtin_text();
    start_tts_playback_c(text);

    // Удаляем ТОЛЬКО здесь, чтобы исключить двойное освобождение
    if (s_tts_timer) {
        lv_timer_del(s_tts_timer);
        s_tts_timer = NULL;
    }
}

static void schedule_tts_after_delay(void)
{
    if (!s_tts_timer) {
        s_tts_timer = lv_timer_create(tts_timer_cb, 1000 /* ms */, NULL);
        // не используем repeat_count=1, сами удалим таймер в callback
        lv_timer_pause(s_tts_timer);      // на всякий случай — стартуем «на паузе»
    }

    lv_timer_set_period(s_tts_timer, 1000);
    lv_timer_reset(s_tts_timer);          // сбрасываем оставшееся время
    lv_timer_resume(s_tts_timer);         // запускаем
}


// ---- Показ кейса: картинка → (1с) → TTS → (после) вопрос/выборы
static void show_case(builtin_text_case_t c)
{
    s_current = c;
    builtin_text_set(c);

// lv_refr_now(NULL);

    apply_image_for_case(c);

    // До конца TTS поля пустые и кнопки скрыты
    lv_label_set_text(ui_LabelQ,  "");
    lv_label_set_text(ui_LabelCh1,"");
    lv_label_set_text(ui_LabelCh2,"");

     lv_obj_add_flag(ui_choice1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_choice2, LV_OBJ_FLAG_HIDDEN);

    schedule_tts_after_delay();
}

// ---- Старт истории
void ui_story_start(void)
{
    show_case(STORY_START_CASE);
}
