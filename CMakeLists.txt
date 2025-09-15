set(COMPONENT_ADD_INCLUDEDIRS src )
file(GLOB SRCS src/*.cpp )
set(COMPONENT_SRCS ${SRCS})

### If you use arduino-esp32 components, please activate next comment line.
# list(APPEND COMPONENT_REQUIRES arduino-esp32)

message(STATUS "LGFX_PPA use components = ${COMPONENT_REQUIRES}")

register_component()
