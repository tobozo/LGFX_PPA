
#include "sdkconfig.h"

#if !defined CONFIG_LGFX_PPA_USE_M5GFX && !defined CONFIG_LGFX_PPA_USE_LOVYANGFX
  // just pick one

  //#define CONFIG_LGFX_PPA_USE_M5GFX
  #define CONFIG_LGFX_PPA_USE_LOVYANGFX

#endif

#if defined CONFIG_LGFX_PPA_USE_M5GFX // Note: M5GFX version must be >= 1.2.20

  #include <M5Unified.hpp>
  #include <LGFX_PPA.hpp>
  #define tft M5.Display
  #pragma message "Compiling with M5GFX"

#elif defined CONFIG_LGFX_PPA_USE_LOVYANGFX

  #include <LovyanGFX.hpp>
  //#define LGFX_PPA_DEVICE_BUFFER_PROVIDER lgfx_ppa_get_panel_config_buffer<Panel_DSI>
  #include <LGFX_PPA.hpp>
  //#include <lgfx_user/LGFX_3.5_RPI_LCD_A.hpp>
  //LGFX_RPI_LDC35A tft;
  #include <lgfx_user/M5Tab5.hpp>
  M5Tab5 tft;
  #pragma message "Compiling with LovyanGFX"

#else

  #error "missing define: CONFIG_LGFX_PPA_USE_LOVYANGFX or CONFIG_LGFX_PPA_USE_M5GFX"

#endif


PPA_Sprite sprite_out;
PPA_Sprite sprite_bg;

PPAFill *ppa_fill;
PPABlend *ppa_blend;
PPASrm *ppa_srm;

float zoomx=4, zoomy=4;
float w, h;

void setup()
{
  #if defined CONFIG_LGFX_PPA_USE_M5GFX
    M5.begin();
  #endif

  #if defined CONFIG_LGFX_PPA_USE_LOVYANGFX
    tft.init();
  #endif

  w = tft.width()/zoomx;
  h = tft.height()/zoomy;

  Serial.println("Hello PPA - Blend/Fill/Scale example");

  // alpha blending needs at least 24bits color
  sprite_out.setColorDepth(24);
  sprite_bg.setColorDepth(24);

  if( !sprite_out.createSprite(w, h) || !sprite_bg.createSprite(w, h) ) {
    Serial.println("Failed to create ppa sprite, halting");
    while(1);
  }

  ppa_fill = new PPAFill(&sprite_bg, false);

  ppa_blend = new PPABlend(&sprite_out, false);
  ppa_blend->setLayers(&sprite_out, &sprite_bg); // NOTE: blend output is also the foreground
  ppa_blend->setFGColorKey(false);
  ppa_blend->setFGAlpha(0.8*0xff);
  ppa_blend->setBGAlpha(0.2*0xff);

  ppa_srm = new PPASrm(&tft, false);
  ppa_srm->setByteSwap(false);

}



void loop()
{
  static uint32_t dw = sprite_bg.width();
  static uint32_t dh = sprite_bg.height();

  lgfx::argb8888_t color = random();
  uint32_t xs=random()%dw, xe=random()%dw, ys=random()%dh, ye=random()%dh;

  if( xs==xe || ys==ye )
    return;

  if( xs>xe )
    std::swap(xs, xe);
  if( ys>ye )
    std::swap(ys, ye);

  int rw = xe-xs, rh = ye-ys;

  if( rw < dw/20 || rh < dh/20 || rw > dw/4 || rh > dh/4 )
    return; // too small or too big

  sprite_bg.clear();
  ppa_fill->fillRect(xs, ys, rw, rh, color.raw);

  ppa_blend->setBGColorKey(true, color.raw, color.raw); // don't blend on the fill color
  ppa_blend->pushImageBlend();

  ppa_srm->pushSRM(&sprite_out, 0, 0, zoomx, zoomy);

}


// esp-idf and/or platformio
#if !defined ARDUINO
extern "C"
{
  void loopTask(void*)
  {
    setup();
    for(;;) {
      loop();
    }
  }
  void app_main()
  {
    xTaskCreatePinnedToCore( loopTask, "loopTask", 8192, NULL, 1, NULL, 1 );
  }
}
#endif
