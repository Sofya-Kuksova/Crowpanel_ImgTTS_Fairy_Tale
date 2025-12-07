// NEW: components/ui/user_name_store.c
#include <stdio.h>
#include <string.h>
#include "user_name_store.h"

/* Файл, в котором храним имя пользователя */
#define USER_NAME_PATH "/spiffs/user_name.txt"

static char s_user_name[USER_NAME_MAX_LEN + 1] = {0};
static int  s_loaded = 0;  // лениво загружаем из файла

/* Внутренняя функция: один раз пытаемся прочитать имя из файла */
static void user_name_try_load(void)
{
    if (s_loaded) {
        return;
    }

    FILE *f = fopen(USER_NAME_PATH, "r");
    if (!f) {
        /* Файл ещё не создан — считается, что имени нет */
        s_user_name[0] = '\0';
        s_loaded = 1;
        return;
    }

    size_t n = fread(s_user_name, 1, USER_NAME_MAX_LEN, f);
    fclose(f);

    /* Удаляем хвостовые переводы строк и пробелы */
    size_t len = n;
    while (len > 0 &&
           (s_user_name[len - 1] == '\n' ||
            s_user_name[len - 1] == '\r' ||
            s_user_name[len - 1] == ' ')) {
        len--;
    }
    s_user_name[len] = '\0';

    s_loaded = 1;
}

const char *user_name_get(void)
{
    user_name_try_load();
    if (s_user_name[0] == '\0') {
        return NULL;
    }
    return s_user_name;
}

void user_name_set(const char *name)
{
    user_name_try_load();  // не обязательно, но безвредно

    if (!name) {
        return;
    }

    /* Копируем в RAM */
    size_t len = strnlen(name, USER_NAME_MAX_LEN);
    memcpy(s_user_name, name, len);
    s_user_name[len] = '\0';

    /* И сразу же сохраняем в SPIFFS */
    FILE *f = fopen(USER_NAME_PATH, "w");
    if (!f) {
        return;
    }
    fwrite(s_user_name, 1, len, f);
    fclose(f);
}
