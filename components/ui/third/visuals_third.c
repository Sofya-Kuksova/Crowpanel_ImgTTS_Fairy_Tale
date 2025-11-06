#include "visuals.h"

/* Replace with your real images and loaders when available. */

extern const lv_img_dsc_t ui_img_third_01_png; void ui_img_third_01_png_load(void); 
extern const lv_img_dsc_t ui_img_third_02_png; void ui_img_third_02_png_load(void); 
extern const lv_img_dsc_t ui_img_third_03_png; void ui_img_third_03_png_load(void); 

const case_visual_t kVisuals[CASE_TXT_COUNT] = {
    [CASE_TXT_01] = { &ui_img_third_01_png, ui_img_third_01_png_load },
    [CASE_TXT_02] = { &ui_img_third_02_png, ui_img_third_02_png_load },
    [CASE_TXT_03] = { &ui_img_third_03_png, ui_img_third_03_png_load },
    /* Remaining entries intentionally left zero-initialized */
};
