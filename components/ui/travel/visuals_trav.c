#include "visuals.h"

extern const lv_img_dsc_t ui_img_020_png; void ui_img_020_png_load(void); // Old Beacon, Norway
extern const lv_img_dsc_t ui_img_03_png;  void ui_img_03_png_load(void);  // Clocks Yard, Prague
extern const lv_img_dsc_t ui_img_04_png;  void ui_img_04_png_load(void);  // Deserted Railway Station, Argentina
extern const lv_img_dsc_t ui_img_05_png;  void ui_img_05_png_load(void);  // Village Covered with Snow, Japan
extern const lv_img_dsc_t ui_img_06_png;  void ui_img_06_png_load(void);  // Bridge across the River, Georgia
extern const lv_img_dsc_t ui_img_07_png;  void ui_img_07_png_load(void);  // Graffiti alley, Lisbon
extern const lv_img_dsc_t ui_img_08_png;  void ui_img_08_png_load(void);  // Desert Well, Morocco
extern const lv_img_dsc_t ui_img_09_png;  void ui_img_09_png_load(void);  // Misty Field, Poland
extern const lv_img_dsc_t ui_img_10_png;  void ui_img_10_png_load(void);  // ...
extern const lv_img_dsc_t ui_img_11_png;  void ui_img_11_png_load(void);
extern const lv_img_dsc_t ui_img_12_png;  void ui_img_12_png_load(void);
extern const lv_img_dsc_t ui_img_13_png;  void ui_img_13_png_load(void);
extern const lv_img_dsc_t ui_img_14_png;  void ui_img_14_png_load(void);
extern const lv_img_dsc_t ui_img_15_png;  void ui_img_15_png_load(void);

const case_visual_t kVisuals[CASE_TXT_COUNT] = {
    [CASE_TXT_01] = { &ui_img_020_png, ui_img_020_png_load }, 
    [CASE_TXT_02] = { &ui_img_03_png,  ui_img_03_png_load  }, 
    [CASE_TXT_03] = { &ui_img_04_png,  ui_img_04_png_load  },
    [CASE_TXT_04] = { &ui_img_05_png,  ui_img_05_png_load  }, 
    [CASE_TXT_05] = { &ui_img_06_png,  ui_img_06_png_load  }, 
    [CASE_TXT_06] = { &ui_img_07_png,  ui_img_07_png_load  }, 
    [CASE_TXT_07] = { &ui_img_08_png,  ui_img_08_png_load  },
    [CASE_TXT_08] = { &ui_img_09_png,  ui_img_09_png_load  }, 
    [CASE_TXT_09] = { &ui_img_10_png,  ui_img_10_png_load  },
    [CASE_TXT_10] = { &ui_img_11_png,  ui_img_11_png_load  },
    [CASE_TXT_11] = { &ui_img_12_png,  ui_img_12_png_load  },
    [CASE_TXT_12] = { &ui_img_13_png,  ui_img_13_png_load  },
    [CASE_TXT_13] = { &ui_img_14_png,  ui_img_14_png_load  },
    [CASE_TXT_14] = { &ui_img_15_png,  ui_img_15_png_load  },
};
