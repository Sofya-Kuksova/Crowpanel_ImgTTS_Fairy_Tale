#pragma once
#include <stdbool.h>
#include "builtin_texts.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // Длинный текст-описание к картинке берём из builtin_texts_* (get_builtin_text()).
    // Здесь — короткие подписи UI + переходы.
    const char *question;    // NULL или "" на финальном уровне
    const char *choice1;     // NULL/"" если финал
    const char *choice2;     // NULL/"" если финал
    bool is_final;           // true на D1..D16
    builtin_text_case_t next1; // куда перейти после choice1 (игнор если финал)
    builtin_text_case_t next2; // куда перейти после choice2 (игнор если финал)
} story_node_t;

// Вернуть узел истории для данного кейса (A,B1,B2,...,D1..D16).
const story_node_t* story_get_node(builtin_text_case_t c);

// Узел A (старт) для удобства
#define STORY_START_CASE  CASE_TXT_01

#ifdef __cplusplus
}
#endif
