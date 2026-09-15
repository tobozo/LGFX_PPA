# set(COMPONENT_ADD_INCLUDEDIRS src )
# file(GLOB SRCS src/*.cpp )
# set(COMPONENT_SRCS ${SRCS})
#
# ### If you use arduino-esp32 components, please activate next comment line.
# # list(APPEND COMPONENT_REQUIRES arduino-esp32)
#
# message(STATUS "LGFX_PPA use components = ${COMPONENT_REQUIRES}")
#
# register_component()

set(COMPONENT_ADD_INCLUDEDIRS src )

file(GLOB_RECURSE SRCS "src/*.cpp" "src/*.c" "src/*.hpp" "src/*.h")
set(COMPONENT_SRCS ${SRCS})

### If you use arduino-esp32 components, please activate next comment line.
# list(APPEND COMPONENT_REQUIRES arduino-esp32)

### Activate either LovyanGFX or M5GFX
if(CONFIG_LGFX_PPA_USE_GFX)
  if(CONFIG_LGFX_PPA_USE_LOVYANGFX)
    list(APPEND COMPONENT_REQUIRES LovyanGFX)
  elseif(CONFIG_LGFX_PPA_USE_M5GFX)
    list(APPEND COMPONENT_REQUIRES M5GFX)
  else()
    message( FATAL_ERROR "No GFX Lib selected, CMake will exit." )
  endif()
else()
  list(APPEND COMPONENT_REQUIRES LovyanGFX)
endif()

message(STATUS "LGFX_PPA use components = ${COMPONENT_REQUIRES}")

register_component()
