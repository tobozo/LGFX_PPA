cmake_minimum_required(VERSION 3.16.0)

# If for some insane reason you're using this project as a boilerplate, change this
set(EXTRA_COMPONENT_DIR "$(PROJECT_PATH)/../../..")

if(CONFIG_LGFX_PPA_USE_GFX)
  if(CONFIG_LGFX_PPA_USE_LOVYANGFX)
    list(APPEND EXTRA_COMPONENT_DIRS "${EXTRA_COMPONENT_DIR}/LovyanGFX")
  elseif(CONFIG_LGFX_PPA_USE_M5GFX)
    list(APPEND EXTRA_COMPONENT_DIRS "${EXTRA_COMPONENT_DIR}/M5GFX")
  else()
    message( FATAL_ERROR "No GFX Lib selected, CMake will exit." )
  endif()
else()
  list(APPEND EXTRA_COMPONENT_DIRS "${EXTRA_COMPONENT_DIR}/LovyanGFX")
endif()

list(APPEND EXTRA_COMPONENT_DIRS "${EXTRA_COMPONENT_DIR}/LGFX_PPA")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(LGFX_Fonts-build-test)
