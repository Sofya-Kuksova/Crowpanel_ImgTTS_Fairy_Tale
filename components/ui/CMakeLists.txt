cmake_minimum_required(VERSION 3.16)

set(UI_SCENARIO_NAME          "fairy_tale")
set(UI_SCENARIO_NAME_LOWER    "fairy_tale")
set(ASSETS_SUBDIR             "assets_fairy_tale")

message(STATUS "UI scenario fixed to: ${UI_SCENARIO_NAME} (assets: ${ASSETS_SUBDIR})")

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
  story_fairy_tale.c
  ui_img_loading_gif.c
)

file(GLOB UI_THIRD_SRCS
  "${CMAKE_CURRENT_LIST_DIR}/fairy_tale/*.c"
)

list(APPEND UI_THIRD_SRCS
  "${CMAKE_CURRENT_LIST_DIR}/builtin_texts_fairy_tale.c"
)

list(REMOVE_DUPLICATES UI_THIRD_SRCS)

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
    gifdec
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC
  UI_SCENARIO_NAME=${UI_SCENARIO_NAME}
  UI_SCENARIO_NAME_LOWER=\"${UI_SCENARIO_NAME_LOWER}\"
  ASSETS_SUBDIR=\"${ASSETS_SUBDIR}\"
)
