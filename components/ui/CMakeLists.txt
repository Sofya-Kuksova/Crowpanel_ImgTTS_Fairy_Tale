cmake_minimum_required(VERSION 3.16)

# ---- Detect scenario from header ------------------------------------------------
set(SCENARIO_HDR "${CMAKE_CURRENT_LIST_DIR}/include/scenario_build.h")
if(NOT EXISTS "${SCENARIO_HDR}")
  message(FATAL_ERROR "Missing file: ${SCENARIO_HDR}")
endif()

file(READ "${SCENARIO_HDR}" _SCEN_DEF_RAW)
string(REPLACE "\r\n" "\n" _SCEN_DEF "${_SCEN_DEF_RAW}")
string(REPLACE "\r"   "\n" _SCEN_DEF "${_SCEN_DEF}")
string(REGEX REPLACE "\n" ";" _SCEN_LINES "${_SCEN_DEF}")

set(_SCEN_SELECTED_NAME  "")
set(_SCEN_SELECTED_COUNT 0)

foreach(_line IN LISTS _SCEN_LINES)
  string(STRIP "${_line}" _L)
  # Allow trailing comments after '1' (e.g., '#define UI_SCENARIO_ALBUM 1 // active')
  if(_L MATCHES "^#define[ \t]+UI_SCENARIO_([A-Za-z0-9_]+)[ \t]+1([ \t]*(//.*)?)?$")
    string(REGEX REPLACE
      "^#define[ \t]+UI_SCENARIO_([A-Za-z0-9_]+)[ \t]+1([ \t]*(//.*)?)?$"
      "\\1" _SCEN_THIS "${_L}")
    set(_SCEN_SELECTED_NAME "${_SCEN_THIS}")
    math(EXPR _SCEN_SELECTED_COUNT "${_SCEN_SELECTED_COUNT}+1")
  endif()
endforeach()

if(NOT _SCEN_SELECTED_COUNT EQUAL 1)
  message(FATAL_ERROR
    "scenario_build.h: define exactly one UI_SCENARIO_* to 1 (found ${_SCEN_SELECTED_COUNT})")
endif()

string(TOLOWER "${_SCEN_SELECTED_NAME}" _SCEN_SELECTED_NAME_LOWER)
message(STATUS "UI scenario selected: ${_SCEN_SELECTED_NAME} (lower='${_SCEN_SELECTED_NAME_LOWER}')")

# ---- Assets subdir convention ---------------------------------------------------
set(_ASSETS_SUBDIR "assets_${_SCEN_SELECTED_NAME_LOWER}")
message(STATUS "UI assets subdir: ${_ASSETS_SUBDIR}")

# ---- Common sources -------------------------------------------------------------
set(UI_COMMON_SRCS
  ui_Screen1.c
  ui.c
  ui_comp_hook.c
  ui_helpers.c
  ui_events.c
  ui_font_Font1.c
  ui_img_manager.c
  ui_img_1049104300.c
  tts_bridge_impl.c
)

set(INCLUDE_DIRS "include")

# ---- Helper: append file if exists (warn otherwise) -----------------------------
function(_ui_append_if_exists path)
  if(EXISTS "${path}")
    list(APPEND UI_SCENARIO_SRCS "${path}")
    set(UI_SCENARIO_SRCS "${UI_SCENARIO_SRCS}" PARENT_SCOPE)
  else()
    message(WARNING "Missing file: ${path}")
  endif()
endfunction()

# ---- Scenario directory: components/ui/<lower>/*.c ------------------------------
set(UI_SCENARIO_SRCS "")
set(_SCEN_DIR "${CMAKE_CURRENT_LIST_DIR}/${_SCEN_SELECTED_NAME_LOWER}")
if(EXISTS "${_SCEN_DIR}")
  file(GLOB _SCEN_DIR_SRCS "${_SCEN_DIR}/*.c")
  list(APPEND UI_SCENARIO_SRCS ${_SCEN_DIR_SRCS})
else()
  message(WARNING "Scenario dir not found: ${_SCEN_DIR}\nCreate it if you keep per-scenario sources there.")
endif()

# ---- Explicit per-scenario sources to avoid link-time gaps ----------------------
if(_SCEN_SELECTED_NAME_LOWER STREQUAL "album")
  _ui_append_if_exists("${CMAKE_CURRENT_LIST_DIR}/builtin_texts_alb.c")
  _ui_append_if_exists("${CMAKE_CURRENT_LIST_DIR}/album/visuals_alb.c")

elseif(_SCEN_SELECTED_NAME_LOWER STREQUAL "travel")
  _ui_append_if_exists("${CMAKE_CURRENT_LIST_DIR}/builtin_texts_trev.c")
  _ui_append_if_exists("${CMAKE_CURRENT_LIST_DIR}/travel/visuals_trav.c")

# -----------------------------------------------------------------------------
# ADD A CUSTOM SCENARIO ("third"):
# -----------------------------------------------------------------------------
# elseif(_SCEN_SELECTED_NAME_LOWER STREQUAL "third")
#   _ui_append_if_exists("${CMAKE_CURRENT_LIST_DIR}/builtin_texts_third.c")
#   _ui_append_if_exists("${CMAKE_CURRENT_LIST_DIR}/third/visuals_third.c")

else()
  message(WARNING "Unknown scenario token '${_SCEN_SELECTED_NAME_LOWER}'.")
endif()

list(REMOVE_DUPLICATES UI_SCENARIO_SRCS)

# ---- Register component ---------------------------------------------------------
idf_component_register(
  SRCS ${UI_COMMON_SRCS} ${UI_SCENARIO_SRCS}
  INCLUDE_DIRS ${INCLUDE_DIRS}
  REQUIRES lvgl__lvgl espressif__esp32_display_panel
)

# ---- Compile definitions for this component (not global) ------------------------
target_compile_definitions(${COMPONENT_LIB} PUBLIC
  UI_SCENARIO_NAME=${_SCEN_SELECTED_NAME}
  UI_SCENARIO_NAME_LOWER=\"${_SCEN_SELECTED_NAME_LOWER}\"
  ASSETS_SUBDIR=\"${_ASSETS_SUBDIR}\"
)

