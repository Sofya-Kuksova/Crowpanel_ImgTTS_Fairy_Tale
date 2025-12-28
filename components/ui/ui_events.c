// components/ui/ui_events.c

#include "ui_events.h"
#include "ui.h"
#include "tts_bridge.h"
#include "builtin_texts.h"
#include "esp_log.h"
#include "esp_system.h" 
#include "lvgl.h"
#include "visuals.h"
#include "esp_heap_caps.h"
#include "story_fairy_tale.h"
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "ui_helpers.h"
#include "user_name_store.h" 


#define UI_FADE_MS            450   // было 350: мягче и заметно “дороже”
#define UI_CHOICE_MS          80
#define UI_SCREEN_3_MS        10
#define UI_SCREEN_FADE_MS     10   // было 50: 50 мс выглядит как “мигание”
#define UI_QUESTION_HOLD_MS  2400   // было 2200: чуть спокойнее
#define UI_OUTRO_DELAY_MS     400   // было 500: чуть больше “паузы”
#define UI_AGAIN_MS           100

#define UI_TALK_START_DELAY_MS  1   // задержка перед запуском TALK после старта аудио
#define UI_TALK_STOP_DELAY_MS   1   // задержка перед остановкой TALK после окончания аудио

static builtin_text_case_t s_current = STORY_START_CASE;
static lv_timer_t *s_tts_timer           = NULL; // старт TTS через 1s
static lv_timer_t *s_after_tts_timer     = NULL; // через 1s после TTS → Screen1
static lv_timer_t *s_question_hold_timer = NULL; // пауза с вопросом

static lv_timer_t *s_outro_tts_timer     = NULL; // ПАУЗА перед финальной репликой (0.5s)

static bool s_intro_mode       = true;  // до первого TTS прогоняем вступление
static bool s_outro_mode       = false; // сейчас проигрываем финальную реплику

/* Новое: состояние экрана настроек */
static int  s_last_active_screen = 0;   // 0 – неизвестно, 1 – Screen1, 2 – Screen2
static bool s_settings_open      = false;

/* Подавить "late finished" после принудительного stop (настройки) до следующего старта TTS */
static bool s_ignore_tts_finished_until_next_start = false;

static bool s_pending_idle_after_talk_stop = false;

static lv_timer_t *s_talk_start_timer    = NULL;
static lv_timer_t *s_talk_stop_timer     = NULL;

static lv_timer_t *s_resume_screen2_timer = NULL;
static builtin_text_case_t s_resume_case  = STORY_START_CASE;
static void resume_screen2_timer_cb(lv_timer_t *t);

static const char* TAG_UI = "ui_events";

/* Вперёд объявления внутренних функций */
static void show_case(builtin_text_case_t c);
static void tts_timer_cb(lv_timer_t* t);
static void schedule_tts_after_delay(void);
static void ui_notify_tts_finished_async(void *arg);

/* Управление изображениями */
static void free_all_images_except(const lv_img_dsc_t* except_s,
                                   const lv_img_dsc_t* except_l);
static void apply_image_for_case_internal(builtin_text_case_t c,
                                          bool need_small,
                                          bool need_large);

/* Новое: пауза/возобновление fade-анимаций вопроса/вариантов */
static void pause_question_and_choice_animations(void);
static void resume_question_and_choice_animations(void);

/* --- АНИМАЦИИ ВОПРОСА И ВАРИАНТОВ (C-версия) --- */

static void start_choices_fade_in(void);   // вперёд объявление
static void question_hold_timer_cb(lv_timer_t *t);
static void question_fade_out_ready_cb(lv_anim_t *a);
static void question_fade_in_ready_cb(lv_anim_t *a);
static void start_question_sequence(const story_node_t *node); 

static void outro_tts_timer_cb(lv_timer_t *t);

/* TALK-GIF delayed start/stop */
static void talk_start_timer_cb(lv_timer_t *t);
static void talk_stop_timer_cb(lv_timer_t *t);
static void schedule_talk_start(void);
static void schedule_talk_stop(void);


static void anim_set_opa_cb(void *var, int32_t v)
{
    lv_obj_t *obj = (lv_obj_t *)var;
    if (!obj) return;
    lv_obj_set_style_opa(obj, (lv_opa_t)v, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void ui_notify_tts_started_async(void *arg)
{
    LV_UNUSED(arg);

    if (s_settings_open) return;

    if (s_outro_mode) {
        ui_bird1_use_talk_gif();
    }

    schedule_talk_start();
}


static void talk_start_timer_cb(lv_timer_t *t)
{
    lv_timer_del(t);
    s_talk_start_timer = NULL;

    if (s_settings_open) return;

    ui_bird_talk_anim_start();
}

static void talk_stop_timer_cb(lv_timer_t *t)
{
    lv_timer_del(t);
    s_talk_stop_timer = NULL;

    // после окончания TTS: для case остаёмся на первом кадре TALK
    ui_bird_talk_anim_stop();

    // для outro: после stop-delay переводим ui_bird1 в idle-gif
    if (s_pending_idle_after_talk_stop) {
        s_pending_idle_after_talk_stop = false;
        ui_bird1_use_idle_gif();
        if (ui_bird1) {
            lv_obj_set_x(ui_bird1, 342);
            lv_obj_set_y(ui_bird1, -204);
        }
    }
}


static void schedule_talk_start(void)
{
    if (s_talk_stop_timer) {
        lv_timer_del(s_talk_stop_timer);
        s_talk_stop_timer = NULL;
    }

    if (s_talk_start_timer) {
        lv_timer_reset(s_talk_start_timer);
    } else {
        s_talk_start_timer = lv_timer_create(talk_start_timer_cb, UI_TALK_START_DELAY_MS, NULL);
    }
}

static void schedule_talk_stop(void)
{
    if (s_talk_start_timer) {
        lv_timer_del(s_talk_start_timer);
        s_talk_start_timer = NULL;
    }

    if (s_talk_stop_timer) {
        lv_timer_reset(s_talk_stop_timer);
    } else {
        s_talk_stop_timer = lv_timer_create(talk_stop_timer_cb, UI_TALK_STOP_DELAY_MS, NULL);
    }
}


void ui_notify_tts_started(void)
{
    lv_async_call(ui_notify_tts_started_async, NULL);
}

/* "Пауза" вопроса и вариантов для Screen1.
 * В LVGL 8 нет lv_anim_pause(), поэтому делаем так:
 *  - останавливаем таймер удержания вопроса;
 *  - просто удаляем текущие анимации opacity.
 * Объекты остаются в том состоянии (opa), в котором были
 * в момент ухода на экран настроек.
 */
static void pause_question_and_choice_animations(void)
{
    /* Пауза таймера удержания вопроса (если он есть) */
    if (s_question_hold_timer) {
        lv_timer_pause(s_question_hold_timer);
    }

    /* Удаляем анимации opacity для всех связанных объектов.
     * Это оставит их "замороженными" в текущей прозрачности.
     */
    if (ui_que2) {
        lv_anim_del(ui_que2, anim_set_opa_cb);
    }
    if (ui_ContainerCh) {
        lv_anim_del(ui_ContainerCh, anim_set_opa_cb);
    }
    if (ui_arr1) {
        lv_anim_del(ui_arr1, anim_set_opa_cb);
    }
    if (ui_arr2) {
        lv_anim_del(ui_arr2, anim_set_opa_cb);
    }
}

static void resume_question_and_choice_animations(void)
{
    /* Берём текущий узел истории по s_current */
    const story_node_t *node = story_get_node(s_current);
    if (!node) {
        return;
    }

    /* На финальном экране у нас своя логика (кнопка end2),
     * вопрос/варианты там не используются — ничего не перезапускаем.
     */
    if (node->is_final) {
        return;
    }

    /* Старый таймер удержания вопроса больше не нужен: мы хотим
     * переиграть последовательность с нуля. Удаляем и обнуляем.
     */
    if (s_question_hold_timer) {
        lv_timer_del(s_question_hold_timer);
        s_question_hold_timer = NULL;
    }

    /* Полностью перезапускаем последовательность вопрос/варианты
     * для того же самого кейса s_current.
     * Внутри start_question_sequence() заново выставятся opa,
     * спрячутся/покажутся объекты и стартуют нужные анимации.
     */
    start_question_sequence(node);
}



static void start_choices_fade_in(void)
{
    /* Берём текущий узел истории по s_current */
    const story_node_t *node = story_get_node(s_current);

    /* На финальном узле сюда, по идее, не попадём (там start_final_end_sequence),
       но на всякий случай защитимся. */
    if (!node || node->is_final) {
        if (ui_ContainerCh) lv_obj_add_flag(ui_ContainerCh, LV_OBJ_FLAG_HIDDEN);
        if (ui_arr1)        lv_obj_add_flag(ui_arr1, LV_OBJ_FLAG_HIDDEN);
        if (ui_arr2)        lv_obj_add_flag(ui_arr2, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    /* Подставляем текст вариантов из story_fairy_tale.c */
    if (ui_Labelch1) {
        lv_label_set_text(ui_Labelch1, node->choice1 ? node->choice1 : "");
    }
    if (ui_Labelch2) {
        lv_label_set_text(ui_Labelch2, node->choice2 ? node->choice2 : "");
    }

    /* Готовим объекты к fade-in: показываем и ставим прозрачность в 0 */
    if (ui_ContainerCh) {
        lv_obj_clear_flag(ui_ContainerCh, LV_OBJ_FLAG_HIDDEN);
        anim_set_opa_cb(ui_ContainerCh, LV_OPA_TRANSP);
    }
    if (ui_arr1) {
        lv_obj_clear_flag(ui_arr1, LV_OBJ_FLAG_HIDDEN);
        anim_set_opa_cb(ui_arr1, LV_OPA_TRANSP);
    }
    if (ui_arr2) {
        lv_obj_clear_flag(ui_arr2, LV_OBJ_FLAG_HIDDEN);
        anim_set_opa_cb(ui_arr2, LV_OPA_TRANSP);
    }

    /* Общий аниматор для трёх объектов (opacity 0 → 255 за 350 мс) */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, anim_set_opa_cb);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a, UI_FADE_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);

    if (ui_ContainerCh) {
        lv_anim_set_var(&a, ui_ContainerCh);
        lv_anim_start(&a);
    }
    if (ui_arr1) {
        lv_anim_set_var(&a, ui_arr1);
        lv_anim_start(&a);
    }
    if (ui_arr2) {
        lv_anim_set_var(&a, ui_arr2);
        lv_anim_start(&a);
    }
}

static void question_fade_out_ready_cb(lv_anim_t *a)
{
    (void)a;
    if (ui_que2) {
        lv_obj_add_flag(ui_que2, LV_OBJ_FLAG_HIDDEN);
    }
    start_choices_fade_in();
}

static void question_hold_timer_cb(lv_timer_t *t)
{
    (void)t;

    if (s_question_hold_timer) {
        lv_timer_del(s_question_hold_timer);
        s_question_hold_timer = NULL;
    }

    if (!ui_que2) return;

    lv_anim_t a_out;
    lv_anim_init(&a_out);
    lv_anim_set_var(&a_out, ui_que2);
    lv_anim_set_exec_cb(&a_out, anim_set_opa_cb);
    lv_anim_set_values(&a_out, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a_out, 350);
    lv_anim_set_ready_cb(&a_out, question_fade_out_ready_cb);
    lv_anim_start(&a_out);
}

static void question_fade_in_ready_cb(lv_anim_t *a)
{
    (void)a;

    if (s_question_hold_timer) {
        lv_timer_del(s_question_hold_timer);
        s_question_hold_timer = NULL;
    }
    /* Вопрос «держится» ~2.2s + анимации ≈ около 3s на экране */
    s_question_hold_timer = lv_timer_create(question_hold_timer_cb, UI_QUESTION_HOLD_MS, NULL);
}

static void start_question_sequence(const story_node_t *node)
{
    if (!ui_que2 || !ui_Labelq2 || !node) return;

    lv_label_set_text(ui_Labelq2, node->question ? node->question : "");

    lv_obj_clear_flag(ui_que2, LV_OBJ_FLAG_HIDDEN);
    _ui_opacity_set(ui_que2, LV_OPA_TRANSP);

    if (ui_ContainerCh) lv_obj_add_flag(ui_ContainerCh, LV_OBJ_FLAG_HIDDEN);
    if (ui_arr1)        lv_obj_add_flag(ui_arr1, LV_OBJ_FLAG_HIDDEN);
    if (ui_arr2)        lv_obj_add_flag(ui_arr2, LV_OBJ_FLAG_HIDDEN);

    lv_anim_t a_in;
    lv_anim_init(&a_in);
    lv_anim_set_var(&a_in, ui_que2);
    lv_anim_set_exec_cb(&a_in, anim_set_opa_cb);
    lv_anim_set_values(&a_in, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a_in, UI_FADE_MS);
    lv_anim_set_path_cb(&a_in, lv_anim_path_ease_in_out);
    lv_anim_set_ready_cb(&a_in, question_fade_in_ready_cb);
    lv_anim_start(&a_in);
}

static void start_final_end_sequence(void)
{
    if (!ui_end2 || !ui_Labelend2) return;

    // Прячем вопрос и выбор
    if (ui_que2)        lv_obj_add_flag(ui_que2, LV_OBJ_FLAG_HIDDEN);
    if (ui_ContainerCh) lv_obj_add_flag(ui_ContainerCh, LV_OBJ_FLAG_HIDDEN);
    if (ui_arr1)        lv_obj_add_flag(ui_arr1, LV_OBJ_FLAG_HIDDEN);
    if (ui_arr2)        lv_obj_add_flag(ui_arr2, LV_OBJ_FLAG_HIDDEN);

    // Подготавливаем кнопку "Try again"
    lv_label_set_text(ui_Labelend2, "Try again");

    lv_obj_clear_flag(ui_end2, LV_OBJ_FLAG_HIDDEN);
    _ui_opacity_set(ui_end2, LV_OPA_TRANSP);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, ui_end2);
    lv_anim_set_exec_cb(&a, anim_set_opa_cb);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a, UI_FADE_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);

    if (s_question_hold_timer) {
        lv_timer_del(s_question_hold_timer);
        s_question_hold_timer = NULL;
    }

    // --- НОВОЕ: говорящая ворона + финальный текст ---

    // Помечаем, что сейчас идёт именно финальная реплика (outro)
    s_outro_mode = true;

    // 1) На Screen1 у ui_bird1 включаем TALK-GIF (тот же, что у ui_bird2)
    ui_bird1_use_talk_gif();
    ui_bird_talk_anim_stop();

    if (ui_bird1) {
        lv_obj_set_x(ui_bird1, 352);
        lv_obj_set_y(ui_bird1, -204);
    }
    
    if (s_outro_tts_timer) {
    lv_timer_del(s_outro_tts_timer);
    s_outro_tts_timer = NULL;
    }
    s_outro_tts_timer = lv_timer_create(outro_tts_timer_cb, UI_OUTRO_DELAY_MS, NULL);

}


static void outro_tts_timer_cb(lv_timer_t *t)
{
    (void)t;

    const char *outro = builtin_get_outro_text();
    if (outro) {
        start_tts_playback_c(outro);
    }

    if (s_outro_tts_timer) {
        lv_timer_del(s_outro_tts_timer);
        s_outro_tts_timer = NULL;
    }
}




/* --- Управление изображениями сказки (small/large) --- */

/* Освобождаем все загруженные изображения сказки, кроме текущей пары */
static void free_all_images_except(const lv_img_dsc_t* except_s,
                                   const lv_img_dsc_t* except_l)
{
    const void *src1 = NULL;
    const void *src2 = NULL;
    if (ui_Img1) src1 = lv_img_get_src(ui_Img1);
    if (ui_Img2) src2 = lv_img_get_src(ui_Img2);

    for (int i = 0; i < CASE_TXT_COUNT; ++i) {
        const case_visual_t *v = &kVisuals[i];

        const lv_img_dsc_t* arr[2] = { v->img_s, v->img_l };
        for (int j = 0; j < 2; ++j) {
            const lv_img_dsc_t *d = arr[j];
            if (!d) continue;
            if (d == except_s || d == except_l) continue;
            if (is_pinned_image(d)) continue;
            if ((const void*)d == src1 || (const void*)d == src2) continue;
            if (!d->data) continue;
            lv_img_cache_invalidate_src((const void*)d);
            ESP_LOGD(TAG_UI, "free image buffer: %p (case=%d, which=%s)", d->data, i, (j == 0 ? "S" : "L"));
            heap_caps_free((void*)d->data);
            ((lv_img_dsc_t*)d)->data = NULL;
            ((lv_img_dsc_t*)d)->data_size = 0;
        }
    }
}

static void ensure_small_image_for_case(builtin_text_case_t c)
{
    if (c < 0 || c >= CASE_TXT_COUNT) {
        return;
    }

    const case_visual_t *v = &kVisuals[c];

    /* 1. Если маленькая уже загружена — просто привяжем её к ui_Img1. */
    if (v->img_s && v->img_s->data) {
        if (ui_Img1) {
            lv_img_set_src(ui_Img1, v->img_s);
        }
        return;
    }

    /* 2. Пытаемся загрузить маленькую картинку. */
    if (v->load_s) {
        ESP_LOGI(TAG_UI, "ensure_small_image_for_case: load SMALL for case %d", (int)c);
        v->load_s();
    }

    /* 3. Если не получилось — выходим, не трогаем старую маленькую. */
    if (!v->img_s || !v->img_s->data) {
        ESP_LOGE(TAG_UI,
                 "ensure_small_image_for_case: SMALL image for case %d is not available",
                 (int)c);
        return;
    }

    /* 4. Привязываем к ui_Img1 (Screen1). */
    if (ui_Img1) {
        lv_img_set_src(ui_Img1, v->img_s);
    }

    /* 5. Доп. подчищаем все остальные кейсы, оставляя current small+large. */
    free_all_images_except(v->img_s, v->img_l);
}


/* Внутренняя функция:
 *  - need_small = true  → грузим маленькую (Screen1)
 *  - need_large = true  → грузим большую  (Screen2)
 * Можно указывать обе, тогда будут загружены обе.
 */
static void apply_image_for_case_internal(builtin_text_case_t c,
                                          bool need_small,
                                          bool need_large)
{
    if (c < 0 || c >= CASE_TXT_COUNT) {
        return;
    }

    const case_visual_t *v = &kVisuals[c];

    const lv_img_dsc_t *img_s = v->img_s;
    const lv_img_dsc_t *img_l = v->img_l;

    /* --- LARGE (Screen2) --- */
    if (need_large) {
        if (v->load_l && (!img_l || !img_l->data)) {
            ESP_LOGI(TAG_UI, "apply_image_for_case_internal: load LARGE for case %d", (int)c);
            v->load_l();
            img_l = v->img_l;
        }

        /* Если большую так и не удалось получить (например, OOM) —
         * не чистим остальные, просто оставляем предыдущую картинку.
         */
        if (!img_l || !img_l->data) {
            ESP_LOGE(TAG_UI,
                     "apply_image_for_case_internal: LARGE image for case %d is not available, keeping previous image",
                     (int)c);
            return;
        }
    }

    /* --- SMALL (Screen1) --- */
    if (need_small) {
        if (v->load_s && (!img_s || !img_s->data)) {
            ESP_LOGI(TAG_UI, "apply_image_for_case_internal: load SMALL for case %d", (int)c);
            v->load_s();
            img_s = v->img_s;
        }

        if (!img_s || !img_s->data) {
            ESP_LOGW(TAG_UI,
                     "apply_image_for_case_internal: SMALL image for case %d is not available",
                     (int)c);
            /* small нам не критичен — продолжаем без него */
        }
    }

    /* --- Привязка к ui_Img1 / ui_Img2 --- */
    if (need_large && ui_Img2 && img_l && img_l->data) {
        lv_img_set_src(ui_Img2, img_l);
    }
    if (need_small && ui_Img1 && img_s && img_s->data) {
        lv_img_set_src(ui_Img1, img_s);
    }

    /* --- Чистим всё лишнее, оставляя текущую пару --- */
    const lv_img_dsc_t *keep_s = need_small ? img_s : v->img_s;
    const lv_img_dsc_t *keep_l = need_large ? img_l : v->img_l;
    free_all_images_except(keep_s, keep_l);
}



/* Публичная оболочка — для debug-кнопки "change":
 * грузит и small, и large.
 */
void apply_image_for_case(builtin_text_case_t c)
{
    apply_image_for_case_internal(c, true, true);
}

/* --- Кнопки "debug" (change / say) --- */

void on_btn_change_pressed(lv_event_t * e)
{
    (void)e;
    builtin_text_next();
    builtin_text_case_t c = builtin_text_get();
    ESP_LOGI(TAG_UI, "Built-in text changed to case #%d", (int)c);
    apply_image_for_case(c);   // debug: обе картинки
}

void on_btn_say_pressed(lv_event_t * e)
{
    (void)e;
    const char* text = get_builtin_text();
    start_tts_playback_c(text);
}

/* --- UART → UI bridge (text to ui_Name) --- */

/* Функция, которая реально трогает LVGL (вызывается в контексте LVGL) */
static void apply_name_async(void *param)
{
    char *name = (char *)param;
    if (!name) {
        return;
    }

    /* НИКАКИХ user_name_set здесь больше нет.
     * Просто отображаем имя в ui_Name.
     */
    if (ui_Name) {
        // если ui_Name — текстовое поле (textarea):
        // lv_textarea_set_text(ui_Name, name);

        // если ui_Name — label, оставь так:
        lv_label_set_text(ui_Name, name);
    }

    free(name);
}



/* Колбэк, который дергает uart_json.c, когда прилетел текст из JSON */
void on_text_update_from_uart(const char *text)
{
    if (!text) return;

    size_t len = strlen(text);
    if (len == 0) return;

    /* Обрезаем хвостовые \r\n, если приходят вместе с текстом */
    while (len > 0 && (text[len - 1] == '\r' || text[len - 1] == '\n')) {
        len--;
    }
    if (len == 0) return;

    /* При желании можно ограничить максимальную длину имени */
    if (len > 63) {
        len = 63;
    }

    char *copy = (char *)malloc(len + 1);
    if (!copy) return;

    memcpy(copy, text, len);
    copy[len] = '\0';

    /* Перепрыгиваем в LVGL-поток: там уже обновим ui_Name */
    lv_async_call(apply_name_async, copy);
}


/* --- Завершение TTS → переход на Screen1 и запуск анимаций --- */

static void after_tts_timer_cb(lv_timer_t *t)
{
    (void)t;

    const story_node_t* node = story_get_node(s_current);
    const bool is_final = (node && node->is_final);

    _ui_screen_change(&ui_Screen1,
                      LV_SCR_LOAD_ANIM_FADE_ON,
                      UI_SCREEN_FADE_MS,
                      0,
                      &ui_Screen1_screen_init);

    if (is_final) {
        start_final_end_sequence();
    } else {
        start_question_sequence(node);
    }

    if (s_after_tts_timer) {
        lv_timer_del(s_after_tts_timer);
        s_after_tts_timer = NULL;
    }
}


static void ui_notify_tts_finished_async(void *arg)
{
    LV_UNUSED(arg);

    s_pending_idle_after_talk_stop = false;

    /* Если TTS был принудительно остановлен (например, при входе в настройки),
     * может прилететь отложенный ui_notify_tts_finished().
     * Не даём ему менять экраны/режимы до следующего старта TTS.
     */
    if (s_ignore_tts_finished_until_next_start) {
        schedule_talk_stop();
        return;
    }

    if (s_settings_open) {
        schedule_talk_stop();
        return;
    }

    if (s_intro_mode) {
        schedule_talk_stop();
        s_intro_mode = false;
        show_case(STORY_START_CASE);
        return;
    }

    if (s_outro_mode) {
        s_outro_mode = false;
        s_pending_idle_after_talk_stop = true;  // <-- важное отличие
        schedule_talk_stop();
        return;
    }

    schedule_talk_stop(); // <-- case: после stop-delay остаёмся на первом кадре TALK

    ensure_small_image_for_case(s_current);

    if (s_after_tts_timer) {
        lv_timer_reset(s_after_tts_timer);
    } else {
        s_after_tts_timer = lv_timer_create(after_tts_timer_cb, 1000, NULL);
    }
}

void ui_notify_tts_finished(void)
{
    lv_async_call(ui_notify_tts_finished_async, NULL);
}

/* --- Выбор варианта истории (кнопки ch1/ch2) --- */

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

/* --- Таймер TTS (запуск через 1 секунду после show_case) --- */

static void tts_timer_cb(lv_timer_t* t)
{
    (void)t;

    const char *text = NULL;
    if (s_intro_mode) text = builtin_get_intro_text();
    else             text = get_builtin_text();

    if (text) {
        /* Новый запуск TTS: снимаем подавление "late finished" от предыдущего stop() */
        s_ignore_tts_finished_until_next_start = false;
        start_tts_playback_c(text);
    } else {
        /* На всякий случай тоже снимаем подавление, чтобы не повиснуть */
        s_ignore_tts_finished_until_next_start = false;
    }


    if (s_tts_timer) {
        lv_timer_del(s_tts_timer);
        s_tts_timer = NULL;
    }
}


static void schedule_tts_after_delay(void)
{
    if (!s_tts_timer) {
        s_tts_timer = lv_timer_create(tts_timer_cb, 1000 /* ms */, NULL);
        lv_timer_pause(s_tts_timer);
    }

    lv_timer_set_period(s_tts_timer, 1000);
    lv_timer_reset(s_tts_timer);
    lv_timer_resume(s_tts_timer);
}

/* --- Показ конкретного кейса истории --- */

static void show_case(builtin_text_case_t c)
{
    ui_bird_talk_anim_stop();
    s_current = c;
    builtin_text_set(c);     // текст для TTS

    /* Сброс таймера удержания вопроса, если он ещё тикает */
    if (s_question_hold_timer) {
        lv_timer_del(s_question_hold_timer);
        s_question_hold_timer = NULL;
    }

    /* *** ВАЖНО: при старте кейса (Screen2 + TTS) грузим ТОЛЬКО БОЛЬШУЮ ***
     *  - это минимизирует фриз во время TTS.
     *  - маленькую загрузим позже, в after_tts_timer_cb().
     */
    apply_image_for_case_internal(c, false, true);

    /* Пока идёт TTS (Screen2), интерфейс вопроса/выбора/финала прячем на Screen1 */
    if (ui_que2)        lv_obj_add_flag(ui_que2, LV_OBJ_FLAG_HIDDEN);
    if (ui_ContainerCh) lv_obj_add_flag(ui_ContainerCh, LV_OBJ_FLAG_HIDDEN);
    if (ui_arr1)        lv_obj_add_flag(ui_arr1, LV_OBJ_FLAG_HIDDEN);
    if (ui_arr2)        lv_obj_add_flag(ui_arr2, LV_OBJ_FLAG_HIDDEN);
    if (ui_end2)        lv_obj_add_flag(ui_end2, LV_OBJ_FLAG_HIDDEN);

    // пока ждём 1s перед TTS — точно НЕ крутим TALK
    ui_bird_talk_anim_stop();

    s_pending_idle_after_talk_stop = false;

    /* Через 1 секунду запустится tts_timer_cb() -> start_tts_playback_c() */
    schedule_tts_after_delay();
}

void ui_story_start(void)
{
    show_case(STORY_START_CASE);
}

/* --- Финальная кнопка "Try again" (ui_end2) --- */

void end_event(lv_event_t * e)
{
    (void)e;

    // 0) Если ещё НЕ успели начать финальную фразу (только висит таймер 0.5 c),
    //    значит никакого outro-TTS нет, можно безопасно сбросить флаг.
    if (s_outro_tts_timer) {
        lv_timer_del(s_outro_tts_timer);
        s_outro_tts_timer = NULL;
        s_outro_mode = false;
    }
    // Если s_outro_tts_timer == NULL и s_outro_mode == true,
    // это означает: финальная реплика УЖЕ проигрывается.
    // В этом случае флаг НЕ трогаем: когда придёт ui_notify_tts_finished()
    // (из tts_stop_playback), ui_notify_tts_finished_async() зайдёт в ветку
    // "if (s_outro_mode) { ... return; }" и тихо проигнорирует это завершение.

    // 1) Принудительно оборвать текущий TTS (если ещё идёт).
    //    Для финальной реплики это приведёт к вызову ui_notify_tts_finished(),
    //    но, как описано выше, он просто сбросит s_outro_mode и НИЧЕГО не сделает.
    tts_stop_playback();      // stop_tts_playback_impl() + ui_bird_talk_anim_stop()

    // 2) На Screen1 вернуть idle-анимацию вместо TALK
    ui_bird1_use_idle_gif();
     if (ui_bird1) {
        lv_obj_set_x(ui_bird1, 342);
        lv_obj_set_y(ui_bird1, -204);
    }

    // 3) ВАЖНО: больше НЕ трогаем s_intro_mode.
    //    Вступительная фраза звучит только один раз — при самом первом старте панели.
    //    Любой Try again (в том числе во время заключительной фразы) сразу начнёт
    //    историю с STORY_START_CASE без приветствия.

    // 4) Чистим таймеры, чтобы не висели старые callbacks
    if (s_tts_timer) {
        lv_timer_del(s_tts_timer);
        s_tts_timer = NULL;
    }
    if (s_after_tts_timer) {
        lv_timer_del(s_after_tts_timer);
        s_after_tts_timer = NULL;
    }
    if (s_question_hold_timer) {
        lv_timer_del(s_question_hold_timer);
        s_question_hold_timer = NULL;
    }

    // 5) Запускаем сказку сначала и переходим на экран воспроизведения (Screen2)
    show_case(STORY_START_CASE);
    _ui_screen_change(&ui_Screen2,
                      LV_SCR_LOAD_ANIM_FADE_ON,
                      UI_AGAIN_MS,
                      0,
                      &ui_Screen2_screen_init);
}

void ui_handle_choice1(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    button_choose_1(e);

    _ui_screen_change(&ui_Screen2,
                      LV_SCR_LOAD_ANIM_FADE_ON,
                      UI_CHOICE_MS,
                      0,
                      &ui_Screen2_screen_init);
}

void ui_handle_choice2(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    button_choose_2(e);

    _ui_screen_change(&ui_Screen2,
                      LV_SCR_LOAD_ANIM_FADE_ON,
                      UI_CHOICE_MS,
                      0,
                      &ui_Screen2_screen_init);
}

static void resume_screen2_timer_cb(lv_timer_t *t)
{
    LV_UNUSED(t);

    if (s_resume_screen2_timer) {
        lv_timer_del(s_resume_screen2_timer);
        s_resume_screen2_timer = NULL;
    }

    /* После выхода из настроек мы всегда хотим “переиграть заново”
     * intro/case (в зависимости от s_intro_mode).
     * show_case() выставит large image + сбросит talk + поставит TTS timer.
     */
    show_case(s_resume_case);

    /* Мы уже на Screen2, show_case сам НЕ переключает экран — это ок. */
}


void ui_handle_settings_from_screen1(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) {
        return;
    }

    s_last_active_screen = 1;
    s_settings_open      = true;

    /* Screen1: паузим fade-анимации вопроса/вариантов */
    pause_question_and_choice_animations();

    /* NEW: если сейчас проигрывается финальная реплика (outro),
     * останавливаем TTS и отменяем таймер, чтобы потом переиграть её с нуля.
     */
    if (s_outro_mode) {
        /* Остановить TTS на модуле HX6538 */
        tts_stop_playback();

        /* Отключить отложенный старт outro, если он ещё не успел сработать */
        if (s_outro_tts_timer) {
            lv_timer_del(s_outro_tts_timer);
            s_outro_tts_timer = NULL;
        }
    }

    _ui_screen_change(&ui_Screen3,
                  LV_SCR_LOAD_ANIM_FADE_ON,
                  UI_SCREEN_3_MS,
                  0,
                  &ui_Screen3_screen_init);

}


void ui_handle_settings_from_screen2(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) {
        return;
    }

    s_last_active_screen = 2;
    s_settings_open      = true;

    /* См. ниже: подавляем "late finished" после stop() до следующего старта TTS */
    s_ignore_tts_finished_until_next_start = true;

    /* Screen2: ПОЛНОСТЬЮ останавливаем TTS и связанные таймеры,
     * чтобы при возврате проиграть текущий кейс заново.
     */
    tts_stop_playback();   // вместо tts_pause_playback()


    if (s_tts_timer) {
        lv_timer_del(s_tts_timer);
        s_tts_timer = NULL;
    }
    if (s_after_tts_timer) {
        lv_timer_del(s_after_tts_timer);
        s_after_tts_timer = NULL;
    }

    _ui_screen_change(&ui_Screen3,
                      LV_SCR_LOAD_ANIM_FADE_ON,
                      UI_SCREEN_3_MS,
                      0,
                      &ui_Screen3_screen_init);
}


void ui_handle_settings_back_from_screen3(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) {
        return;
    }

    s_settings_open = false;

    if (s_last_active_screen == 1) {
    /* Возврат на Screen1 */
    _ui_screen_change(&ui_Screen1,
                      LV_SCR_LOAD_ANIM_FADE_ON,
                      UI_SCREEN_3_MS,
                      0,
                      &ui_Screen1_screen_init);

    /* Возобновляем fade-анимации вопроса и вариантов
     * (для финального узла внутри resume_question_and_choice_animations()
     * мы уже сделали ранний return через node->is_final, так что там ничего
     * лишнего не произойдёт).
     */
    resume_question_and_choice_animations();

    /* NEW: если мы на финальном экране (s_outro_mode == true),
     * значит outro было прервано настройками.
     * Переигрываем финальную реплику с нуля: создаём заново таймер,
     * который через 0.5 s запустит builtin_get_outro_text().
     */
    if (s_outro_mode && s_outro_tts_timer == NULL) {
        /* ПАУЗА перед финальной репликой (0.5s) — как в start_final_end_sequence() */
        s_outro_tts_timer = lv_timer_create(outro_tts_timer_cb, UI_OUTRO_DELAY_MS, NULL);
    }
} else if (s_last_active_screen == 2) {
    _ui_screen_change(&ui_Screen2,
                      LV_SCR_LOAD_ANIM_FADE_ON,
                      UI_SCREEN_3_MS,
                      0,
                      &ui_Screen2_screen_init);

    /* ВАЖНО:
     * - если был прерван INTRO: s_intro_mode всё ещё true → tts_timer_cb возьмёт intro текст
     * - если был прерван CASE:  s_intro_mode false → tts_timer_cb возьмёт текст текущего кейса
     *
     * Но чтобы восстановить “как раньше” (полный перезапуск кейса),
     * мы обязаны снова вызвать show_case(), а не только schedule_tts_after_delay().
     *
     * Делать это нужно с небольшой задержкой, чтобы Screen2 init успел создать ui_Img2.
     */
    s_resume_case = (s_intro_mode ? STORY_START_CASE : s_current);

    if (s_resume_screen2_timer) {
        lv_timer_del(s_resume_screen2_timer);
        s_resume_screen2_timer = NULL;
    }
    s_resume_screen2_timer = lv_timer_create(resume_screen2_timer_cb,
                                             UI_SCREEN_3_MS + 5,
                                             NULL);
    }
    else {
        /* fallback, если что-то пошло не так */
        _ui_screen_change(&ui_Screen2,
                          LV_SCR_LOAD_ANIM_FADE_ON,
                          UI_SCREEN_3_MS,
                          0,
                          &ui_Screen2_screen_init);
    }
}


/* --- Заглушки под события, которые сгенерировал SquareLine --- */

void reload_app(lv_event_t * e)
{
    (void)e;

    /* Самый простой и надёжный способ "начать всё с начала" —
     * перезагрузить микроконтроллер. При следующем старте:
     *  - SPIFFS монтируется;
     *  - user_name_get() прочитает имя из /spiffs/user_name.txt;
     *  - builtin_get_intro_text() сразу начнёт использовать
     *    персонализированную фразу.
     */
    esp_restart();
}


void save_name(lv_event_t * e)
{
    (void)e;

    if (!ui_Name) {
        return;
    }

    /* ui_Name у нас — lv_label, поэтому берём текст именно так */
    const char *text = lv_label_get_text(ui_Name);
    if (!text) {
        return;
    }

    /* Обрезаем до USER_NAME_MAX_LEN и нормализуем */
    char buf[USER_NAME_MAX_LEN + 1];
    size_t len = strnlen(text, USER_NAME_MAX_LEN);
    memcpy(buf, text, len);
    buf[len] = '\0';

    /* Сохраняем в RAM + /spiffs/user_name.txt */
    user_name_set(buf);
}

