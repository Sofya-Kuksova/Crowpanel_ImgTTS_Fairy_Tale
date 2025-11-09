#include "builtin_texts.h"
#include <stdatomic.h>

/* Scenario: THIRD — 3 sample texts (you can add more or less). */
static const char* kThirdTexts[] = {
    // CASE_TXT_01
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

    // CASE_TXT_02
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

    // CASE_TXT_03
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

      // CASE_TXT_04
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

    // CASE_TXT_05
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

    // CASE_TXT_06
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

      // CASE_TXT_07
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

    // CASE_TXT_08
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

    // CASE_TXT_09
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

      // CASE_TXT_10
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

    // CASE_TXT_11
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

    // CASE_TXT_12
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

      // CASE_TXT_13
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

    // CASE_TXT_14
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

    // CASE_TXT_15
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

      // CASE_TXT_16
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

    // CASE_TXT_17
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

    // CASE_TXT_18
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

       // CASE_TXT_19
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

    // CASE_TXT_20
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

    // CASE_TXT_21
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

         // CASE_TXT_22
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

    // CASE_TXT_23
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

    // CASE_TXT_24
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",
    
    // CASE_TXT_25
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

    // CASE_TXT_26
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

    // CASE_TXT_27
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

    // CASE_TXT_28
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

    // CASE_TXT_29
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

    // CASE_TXT_30
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",

     // CASE_TXT_31
    "Row 1.\n"
    "Row 2.\n"
    "Row 3.",
};

static _Atomic(builtin_text_case_t) s_current_case = CASE_TXT_01;

static inline int clamp_index(int idx, int n) {
    if (n <= 0) return 0;
    if (idx < 0) return 0;
    if (idx >= n) return n - 1;
    return idx;
}

int builtin_text_count(void) {
    return (int)(sizeof(kThirdTexts) / sizeof(kThirdTexts[0]));
}

const char* get_builtin_text(void) {
    int n = builtin_text_count();
    if (n <= 0) return "";
    int i = clamp_index((int)s_current_case, n);
    return kThirdTexts[i];
}

void builtin_text_next(void) {
    int n = builtin_text_count();
    if (n <= 0) return;
    int cur = (int)s_current_case;
    int nxt = (cur + 1) % n;
    s_current_case = (builtin_text_case_t)nxt;
}

void builtin_text_set(builtin_text_case_t c) {
    int n = builtin_text_count();
    if (n <= 0) return;
    s_current_case = (builtin_text_case_t)clamp_index((int)c, n);
}

builtin_text_case_t builtin_text_get(void) {
    return s_current_case;
}
