cmake_minimum_required(VERSION 3.16)

# ------------------------------------------------------------------------------
# Single-scenario build: фиксируем проект на сценарии "third"
# Файл scenario_build.h больше не используется.
# Ресурсы берём всегда из spiffs_root/assets_third
# ------------------------------------------------------------------------------

set(UI_SCENARIO_NAME          "fairy_tale")
set(UI_SCENARIO_NAME_LOWER    "fairy_tale")
set(ASSETS_SUBDIR             "assets_fairy_tale")

message(STATUS "UI scenario fixed to: ${UI_SCENARIO_NAME} (assets: ${ASSETS_SUBDIR})")

# ------------------------------------------------------------------------------
# Источники общего UI (не зависят от сценария)
# При необходимости дополните список своими файлами .c
# ------------------------------------------------------------------------------
set(UI_COMMON_SRCS
  ui_Screen1.c
  ui.c
  ui_comp_hook.c
  ui_helpers.c
  ui_events.c
  ui_font_Font1.c
  ui_font_Font2.c
  ui_img_manager.c
  ui_img_arrow_png.c
  tts_bridge_impl.c
)

# ------------------------------------------------------------------------------
# Источники сценария "fairy_tale"
# Подхватим всё из components/ui/fairy_tale/*.c и явно добавим builtin_texts_fairy_tale.c
# ------------------------------------------------------------------------------
file(GLOB UI_THIRD_SRCS
  "${CMAKE_CURRENT_LIST_DIR}/fairy_tale/*.c"
)

list(APPEND UI_THIRD_SRCS
  "${CMAKE_CURRENT_LIST_DIR}/builtin_texts_fairy_tale.c"
)

# Удалим дубликаты на всякий случай
list(REMOVE_DUPLICATES UI_THIRD_SRCS)

# ------------------------------------------------------------------------------
# Регистрация компонента ESP-IDF
# ------------------------------------------------------------------------------
idf_component_register(
  SRCS
    ${UI_COMMON_SRCS}
    ${UI_THIRD_SRCS}
  INCLUDE_DIRS
    "include"
    "fairy_tale"
  REQUIRES
    lvgl__lvgl
    espressif__esp32_display_panel
)

# ------------------------------------------------------------------------------
# Компиляторные дефайны (локальны для этого компонента)
# Используются в рантайме для построения путей и логов.
# ------------------------------------------------------------------------------
target_compile_definitions(${COMPONENT_LIB} PUBLIC
  UI_SCENARIO_NAME=${UI_SCENARIO_NAME}
  UI_SCENARIO_NAME_LOWER=\"${UI_SCENARIO_NAME_LOWER}\"
  ASSETS_SUBDIR=\"${ASSETS_SUBDIR}\"
)
