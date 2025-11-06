cmake_minimum_required(VERSION 3.5)

set(COMPONENTS "main")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(proj)

set(SCENARIO_HDR "${CMAKE_SOURCE_DIR}/components/ui/include/scenario_build.h")
if(NOT EXISTS "${SCENARIO_HDR}")
    message(FATAL_ERROR "Missing file: ${SCENARIO_HDR}")
endif()

file(READ "${SCENARIO_HDR}" _SCEN_DEF_RAW)
string(REPLACE "\r\n" "\n" _SCEN_DEF "${_SCEN_DEF_RAW}")
string(REPLACE "\r"   "\n" _SCEN_DEF "${_SCEN_DEF}")
string(REGEX REPLACE "\n" ";" _SCEN_LINES "${_SCEN_DEF}")

set(_SCEN_SELECTED_NAME "")
set(_SCEN_SELECTED_COUNT 0)
foreach(_line IN LISTS _SCEN_LINES)
    string(STRIP "${_line}" _L)
    if(_L MATCHES "^#define[ \t]+UI_SCENARIO_([A-Za-z0-9_]+)[ \t]+1[ \t]*$")
        string(REGEX REPLACE "^#define[ \t]+UI_SCENARIO_([A-Za-z0-9_]+)[ \t]+1[ \t]*$" "\\1" _SCEN_THIS "${_L}")
        set(_SCEN_SELECTED_NAME "${_SCEN_THIS}")
        math(EXPR _SCEN_SELECTED_COUNT "${_SCEN_SELECTED_COUNT}+1")
    endif()
endforeach()

if(NOT _SCEN_SELECTED_COUNT EQUAL 1)
    message(FATAL_ERROR "scenario_build.h: define exactly one UI_SCENARIO_* to 1 (found ${_SCEN_SELECTED_COUNT})")
endif()

string(TOLOWER "${_SCEN_SELECTED_NAME}" _SCEN_SELECTED_NAME_LOWER)
message(STATUS "UI_SCENARIO (from scenario_build.h) = ${_SCEN_SELECTED_NAME_LOWER}")

set(SPIFFS_ROOT_SRC "${CMAKE_SOURCE_DIR}/spiffs_root")
set(SPIFFS_STAGE_DIR "${CMAKE_BINARY_DIR}/spiffs_stage")

file(REMOVE_RECURSE "${SPIFFS_STAGE_DIR}")
file(MAKE_DIRECTORY "${SPIFFS_STAGE_DIR}")
file(MAKE_DIRECTORY "${SPIFFS_STAGE_DIR}/assets")

if(EXISTS "${SPIFFS_ROOT_SRC}/assets")
    file(COPY "${SPIFFS_ROOT_SRC}/assets/" DESTINATION "${SPIFFS_STAGE_DIR}/assets")
endif()

set(_ASSETS_SCEN_DIR "${SPIFFS_ROOT_SRC}/assets_${_SCEN_SELECTED_NAME_LOWER}")

set(_LEGACY_DIR "")
if(NOT EXISTS "${_ASSETS_SCEN_DIR}")
    if(_SCEN_SELECTED_NAME_LOWER STREQUAL "album" AND EXISTS "${SPIFFS_ROOT_SRC}/assets_alb")
        set(_LEGACY_DIR "${SPIFFS_ROOT_SRC}/assets_alb")
    elseif(_SCEN_SELECTED_NAME_LOWER STREQUAL "travel" AND EXISTS "${SPIFFS_ROOT_SRC}/assets_trav")
        set(_LEGACY_DIR "${SPIFFS_ROOT_SRC}/assets_trav")
    endif()
endif()

if(EXISTS "${_ASSETS_SCEN_DIR}")
    file(COPY "${_ASSETS_SCEN_DIR}" DESTINATION "${SPIFFS_STAGE_DIR}")
elseif(NOT "${_LEGACY_DIR}" STREQUAL "")
    message(WARNING
        "Using legacy assets folder for scenario '${_SCEN_SELECTED_NAME_LOWER}': ${_LEGACY_DIR}\n"
        "Consider migrating to: ${SPIFFS_ROOT_SRC}/assets_${_SCEN_SELECTED_NAME_LOWER}")
    file(COPY "${_LEGACY_DIR}" DESTINATION "${SPIFFS_STAGE_DIR}")
else()
    message(FATAL_ERROR
        "Assets folder for scenario not found.\n"
        "Expected: ${_ASSETS_SCEN_DIR}\n"
        "Create it and put your scenario-specific files there.")
endif()

spiffs_create_partition_image(spiffs ${SPIFFS_STAGE_DIR} FLASH_IN_PROJECT)
