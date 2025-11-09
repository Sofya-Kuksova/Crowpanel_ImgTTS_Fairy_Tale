cmake_minimum_required(VERSION 3.5)

set(COMPONENTS "main")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(proj)

# --- Build SPIFFS only from two folders: assets/ and assets_fairy_tale/ ---

set(SPIFFS_SRC_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/spiffs_root")
set(SPIFFS_STAGE_DIR "${CMAKE_BINARY_DIR}/spiffs_stage")

# Проверим, что исходные папки существуют
if(NOT EXISTS "${SPIFFS_SRC_DIR}/assets")
  message(FATAL_ERROR "Missing folder: ${SPIFFS_SRC_DIR}/assets")
endif()
if(NOT EXISTS "${SPIFFS_SRC_DIR}/assets_fairy_tale")
  message(FATAL_ERROR "Missing folder: ${SPIFFS_SRC_DIR}/assets_fairy_tale")
endif()

# Подготовим staging-директорию ровно с двумя нужными папками
add_custom_command(
  OUTPUT "${SPIFFS_STAGE_DIR}/.prepared"
  COMMAND ${CMAKE_COMMAND} -E rm -rf "${SPIFFS_STAGE_DIR}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${SPIFFS_STAGE_DIR}"
  # копируем целиком обе папки, имена сохраняются
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${SPIFFS_SRC_DIR}/assets"
          "${SPIFFS_STAGE_DIR}/assets"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${SPIFFS_SRC_DIR}/assets_fairy_tale"
          "${SPIFFS_STAGE_DIR}/assets_fairy_tale"
  COMMAND ${CMAKE_COMMAND} -E touch "${SPIFFS_STAGE_DIR}/.prepared"
  VERBATIM
)

add_custom_target(spiffs_stage ALL DEPENDS "${SPIFFS_STAGE_DIR}/.prepared")

# Упаковываем именно staging (внутри только assets/ и assets_fairy_tale/)
spiffs_create_partition_image(spiffs "${SPIFFS_STAGE_DIR}" FLASH_IN_PROJECT)

# Гарантируем порядок: сначала подготовить staging, потом собирать образ
add_dependencies(spiffs_spiffs_bin spiffs_stage)


