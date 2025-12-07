// NEW: components/ui/include/user_name_store.h
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Максимальная длина имени (как у ui_Name) */
#define USER_NAME_MAX_LEN 63

/* Получить сохранённое имя.
 * Может вернуть NULL или пустую строку, если имя ещё не задано.
 * Внутри лениво подгружает имя из файла /spiffs/user_name.txt.
 */
const char *user_name_get(void);

/* Установить и сохранить имя.
 * Имя сохраняется в RAM и в файле /spiffs/user_name.txt,
 * чтобы переживать перезапуск панели.
 */
void user_name_set(const char *name);

#ifdef __cplusplus
}
#endif
