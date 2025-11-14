#pragma once
#include "lvgl.h"
#include <stdbool.h>
#include "builtin_texts.h"

typedef void (*img_loader_t)(void);
typedef struct { const lv_img_dsc_t* img; img_loader_t load; } case_visual_t;

extern lv_img_dsc_t ui_img_arrow_png;
void ui_img_arrow_png_load(void);

extern const case_visual_t kVisuals[CASE_TXT_COUNT];

static inline bool is_pinned_image(const lv_img_dsc_t* d)
{
    return d == &ui_img_arrow_png;
}
