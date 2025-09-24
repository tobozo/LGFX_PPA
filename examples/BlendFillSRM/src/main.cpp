
#include <M5Unified.hpp>
#include <LGFX_PPA.hpp>

#define tft M5.Display

lgfx::PPA_Sprite sprite_out;
lgfx::PPA_Sprite sprite_bg;

lgfx::PPAFill *ppa_fill;
lgfx::PPABlend *ppa_blend;
lgfx::PPASrm *ppa_srm;


float zoomx=8, zoomy=8;
float w, h;

void setup()
{

  M5.begin();

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

  ppa_fill = new lgfx::PPAFill(&sprite_bg, false);

  ppa_blend = new lgfx::PPABlend(&sprite_out, false);
  ppa_blend->setLayers(&sprite_out, &sprite_bg); // NOTE: blend output is also the foreground
  ppa_blend->setFGColorKey(false);
  ppa_blend->setFGAlpha(0.8*0xff);
  ppa_blend->setBGAlpha(0.2*0xff);

  ppa_srm = new lgfx::PPASrm(&tft, false);
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
