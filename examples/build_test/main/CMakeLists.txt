
if(CONFIG_LGFX_PPA_USE_GFX)
  if(CONFIG_LGFX_PPA_USE_LOVYANGFX)
    set(GFX_LIB LovyanGFX)
  elseif(CONFIG_LGFX_PPA_USE_M5GFX)
    set(GFX_LIB M5GFX esp_adc driver)
  else()
    message( FATAL_ERROR "No GFX Lib selected, CMake will exit." )
  endif()
else()
  set(GFX_LIB LovyanGFX)
endif()

idf_component_register(
  SRCS "main.cpp"
  INCLUDE_DIRS  "."
  REQUIRES LGFX_PPA ${GFX_LIB})
