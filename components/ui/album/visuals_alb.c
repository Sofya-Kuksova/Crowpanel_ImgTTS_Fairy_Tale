#include "visuals.h"

extern const lv_img_dsc_t ui_img_01_a_png; void ui_img_01_a_png_load(void);
extern const lv_img_dsc_t ui_img_02_a_png; void ui_img_02_a_png_load(void);
extern const lv_img_dsc_t ui_img_04_png;   void ui_img_04_png_load(void);
extern const lv_img_dsc_t ui_img_05_a_png; void ui_img_05_a_png_load(void);
extern const lv_img_dsc_t ui_img_06_a_png; void ui_img_06_a_png_load(void);
extern const lv_img_dsc_t ui_img_07_png;   void ui_img_07_png_load(void);
extern const lv_img_dsc_t ui_img_08_a_png; void ui_img_08_a_png_load(void);
extern const lv_img_dsc_t ui_img_09_a_png; void ui_img_09_a_png_load(void);
extern const lv_img_dsc_t ui_img_10_b_png; void ui_img_10_b_png_load(void);
extern const lv_img_dsc_t ui_img_11_a_png; void ui_img_11_a_png_load(void);
extern const lv_img_dsc_t ui_img_12_a_png; void ui_img_12_a_png_load(void);
extern const lv_img_dsc_t ui_img_13_a_png; void ui_img_13_a_png_load(void);
extern const lv_img_dsc_t ui_img_14_b_png; void ui_img_14_b_png_load(void);
extern const lv_img_dsc_t ui_img_15_a_png; void ui_img_15_a_png_load(void);

const case_visual_t kVisuals[CASE_TXT_COUNT] = {
    [CASE_TXT_01] = { &ui_img_01_a_png, ui_img_01_a_png_load },
    [CASE_TXT_02] = { &ui_img_02_a_png, ui_img_02_a_png_load },
    [CASE_TXT_03] = { &ui_img_04_png,   ui_img_04_png_load   },
    [CASE_TXT_04] = { &ui_img_05_a_png, ui_img_05_a_png_load },
    [CASE_TXT_05] = { &ui_img_06_a_png, ui_img_06_a_png_load },
    [CASE_TXT_06] = { &ui_img_07_png,   ui_img_07_png_load   },
    [CASE_TXT_07] = { &ui_img_08_a_png, ui_img_08_a_png_load },
    [CASE_TXT_08] = { &ui_img_09_a_png, ui_img_09_a_png_load },
    [CASE_TXT_09] = { &ui_img_10_b_png, ui_img_10_b_png_load },
    [CASE_TXT_10] = { &ui_img_11_a_png, ui_img_11_a_png_load },
    [CASE_TXT_11] = { &ui_img_12_a_png, ui_img_12_a_png_load },
    [CASE_TXT_12] = { &ui_img_13_a_png, ui_img_13_a_png_load },
    [CASE_TXT_13] = { &ui_img_14_b_png,   ui_img_14_b_png_load }, 
    [CASE_TXT_14] = { &ui_img_15_a_png,   ui_img_15_a_png_load }, 
};
