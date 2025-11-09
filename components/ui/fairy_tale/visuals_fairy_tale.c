#include "visuals.h"

/* Replace with your real images and loaders when available. */

extern const lv_img_dsc_t ui_img_a_png; void ui_img_a_png_load(void); 
extern const lv_img_dsc_t ui_img_b1_png; void ui_img_b1_png_load(void); 
extern const lv_img_dsc_t ui_img_b2_png; void ui_img_b2_png_load(void); 
extern const lv_img_dsc_t ui_img_b1_1_png; void ui_img_b1_1_png_load(void); 
extern const lv_img_dsc_t ui_img_b1_2_png; void ui_img_b1_2_png_load(void); 
extern const lv_img_dsc_t ui_img_b2_1_png; void ui_img_b2_1_png_load(void); 
extern const lv_img_dsc_t ui_img_b2_2_png; void ui_img_b2_2_png_load(void); 
extern const lv_img_dsc_t ui_img_b1_1_1_png; void ui_img_b1_1_1_png_load(void); 
extern const lv_img_dsc_t ui_img_b1_1_2_png; void ui_img_b1_1_2_png_load(void); 
extern const lv_img_dsc_t ui_img_b1_2_1_png; void ui_img_b1_2_1_png_load(void); 
extern const lv_img_dsc_t ui_img_b1_2_2_png; void ui_img_b1_2_2_png_load(void); 
extern const lv_img_dsc_t ui_img_b2_2_2_png; void ui_img_b2_2_2_png_load(void); 
extern const lv_img_dsc_t ui_img_b2_1_2_png; void ui_img_b2_1_2_png_load(void); 
extern const lv_img_dsc_t ui_img_b2_2_1_png; void ui_img_b2_2_1_png_load(void); 
extern const lv_img_dsc_t ui_img_b2_1_1_png; void ui_img_b2_1_1_png_load(void); 
extern const lv_img_dsc_t ui_img_d1_png; void ui_img_d1_png_load(void); 
extern const lv_img_dsc_t ui_img_d2_png; void ui_img_d2_png_load(void); 
extern const lv_img_dsc_t ui_img_d3_png; void ui_img_d3_png_load(void); 
extern const lv_img_dsc_t ui_img_d4_png; void ui_img_d4_png_load(void); 
extern const lv_img_dsc_t ui_img_d5_png; void ui_img_d5_png_load(void); 
extern const lv_img_dsc_t ui_img_d6_png; void ui_img_d6_png_load(void); 
extern const lv_img_dsc_t ui_img_d7_png; void ui_img_d7_png_load(void); 
extern const lv_img_dsc_t ui_img_d8_png; void ui_img_d8_png_load(void); 
extern const lv_img_dsc_t ui_img_d9_png; void ui_img_d9_png_load(void); 
extern const lv_img_dsc_t ui_img_d10_png; void ui_img_d10_png_load(void); 
extern const lv_img_dsc_t ui_img_d11_png; void ui_img_d11_png_load(void); 
extern const lv_img_dsc_t ui_img_d12_png; void ui_img_d12_png_load(void); 
extern const lv_img_dsc_t ui_img_d13_png; void ui_img_d13_png_load(void); 
extern const lv_img_dsc_t ui_img_d14_png; void ui_img_d14_png_load(void); 
extern const lv_img_dsc_t ui_img_d15_png; void ui_img_d15_png_load(void); 
extern const lv_img_dsc_t ui_img_d16_png; void ui_img_d16_png_load(void); 


const case_visual_t kVisuals[CASE_TXT_COUNT] = {
    [CASE_TXT_01] = { &ui_img_a_png, ui_img_a_png_load },
    [CASE_TXT_02] = { &ui_img_b1_png, ui_img_b1_png_load },
    [CASE_TXT_03] = { &ui_img_b2_png, ui_img_b2_png_load },
    [CASE_TXT_04] = { &ui_img_b1_1_png, ui_img_b1_1_png_load },
    [CASE_TXT_05] = { &ui_img_b1_2_png, ui_img_b1_2_png_load },
    [CASE_TXT_06] = { &ui_img_b2_1_png, ui_img_b2_1_png_load },
    [CASE_TXT_07] = { &ui_img_b2_2_png, ui_img_b2_2_png_load },
    [CASE_TXT_08] = { &ui_img_b1_1_1_png, ui_img_b1_1_1_png_load },
    [CASE_TXT_09] = { &ui_img_b1_1_2_png, ui_img_b1_1_2_png_load },
    [CASE_TXT_10] = { &ui_img_b1_2_1_png, ui_img_b1_2_1_png_load },
    [CASE_TXT_11] = { &ui_img_b1_2_2_png, ui_img_b1_2_2_png_load },
    [CASE_TXT_12] = { &ui_img_b2_1_1_png, ui_img_b2_1_1_png_load },
    [CASE_TXT_13] = { &ui_img_b2_1_2_png, ui_img_b2_1_2_png_load },
    [CASE_TXT_14] = { &ui_img_b2_2_1_png, ui_img_b2_2_1_png_load },
    [CASE_TXT_15] = { &ui_img_b2_2_2_png, ui_img_b2_2_2_png_load },
    [CASE_TXT_16] = { &ui_img_d1_png, ui_img_d1_png_load },
    [CASE_TXT_17] = { &ui_img_d2_png, ui_img_d2_png_load },
    [CASE_TXT_18] = { &ui_img_d3_png, ui_img_d3_png_load },
    [CASE_TXT_19] = { &ui_img_d4_png, ui_img_d4_png_load },
    [CASE_TXT_20] = { &ui_img_d5_png, ui_img_d5_png_load },
    [CASE_TXT_21] = { &ui_img_d6_png, ui_img_d6_png_load },
    [CASE_TXT_22] = { &ui_img_d7_png, ui_img_d7_png_load },
    [CASE_TXT_23] = { &ui_img_d8_png, ui_img_d8_png_load },
    [CASE_TXT_24] = { &ui_img_d9_png, ui_img_d9_png_load },
    [CASE_TXT_25] = { &ui_img_d10_png, ui_img_d10_png_load },
    [CASE_TXT_26] = { &ui_img_d11_png, ui_img_d11_png_load },
    [CASE_TXT_27] = { &ui_img_d12_png, ui_img_d12_png_load },
    [CASE_TXT_28] = { &ui_img_d13_png, ui_img_d13_png_load },
    [CASE_TXT_29] = { &ui_img_d14_png, ui_img_d14_png_load },
    [CASE_TXT_30] = { &ui_img_d15_png, ui_img_d15_png_load },
    [CASE_TXT_31] = { &ui_img_d16_png, ui_img_d16_png_load },
    
    
    /* Remaining entries intentionally left zero-initialized */
};
