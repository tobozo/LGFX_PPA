
#include <M5Unified.hpp>
#include <LGFX_PPA.hpp>

#define tft M5.Display

lgfx::PPA_Sprite ppa_fgsprite;
lgfx::PPA_Sprite ppa_bgsprite;
lgfx::PPA_Sprite ppa_dstsprite;
lgfx::PPABlend *ppa_blend;

void setup()
{

  M5.begin();
  tft.setRotation(1); // landscape mode

  Serial.println("Hello PPA - Blend example");

  // alpha blending needs at least 24bits color
  ppa_dstsprite.setColorDepth(24);
  ppa_fgsprite.setColorDepth(24);
  ppa_bgsprite.setColorDepth(24);

  // fg, bg and destination must have the same size
  if( !ppa_dstsprite.createSprite(240, 240)
   || !ppa_fgsprite.createSprite(240, 240)
   || !ppa_bgsprite.createSprite(240, 240) ) {
    Serial.println("Failed to create ppa sprite, halting");
    while(1);
  }

  ppa_fgsprite.fillGradientRect(4, 8, ppa_fgsprite.width()-8, ppa_fgsprite.height()-16, 0xff0080u, 0x00ff80u, lgfx::RADIAL);
  ppa_fgsprite.setFont(&FreeMonoBold24pt7b);
  ppa_fgsprite.setTextSize(4);
  ppa_fgsprite.setTextColor(0x000000u);
  ppa_fgsprite.setTextDatum(MC_DATUM);
  ppa_fgsprite.drawString("fg", ppa_fgsprite.width()/2, ppa_fgsprite.height()/2);

  ppa_bgsprite.fillGradientRect(4, 8, ppa_bgsprite.width()-8, ppa_bgsprite.height()-16, 0x00ffffu, 0x800080u, lgfx::RADIAL);
  ppa_bgsprite.setFont(&FreeMonoBold24pt7b);
  ppa_bgsprite.setTextSize(4);
  ppa_bgsprite.setTextColor(0xffffffu);
  ppa_bgsprite.setTextDatum(MC_DATUM);
  ppa_bgsprite.drawString("bg", ppa_bgsprite.width()/2, ppa_bgsprite.height()/2);

  ppa_fgsprite.pushSprite(&tft, 0, 0);
  ppa_bgsprite.pushSprite(&tft, tft.width()-ppa_fgsprite.width()-1, 0);

  ppa_blend = new lgfx::PPABlend(&ppa_dstsprite);

}



void loop()
{

  static uint8_t ialpha = 0;
  static int8_t idir = 2;

  if( ialpha+idir <= 0 || ialpha+idir >= 0xff )
    idir = -idir;

  ialpha += idir;

  if(ppa_blend->available())
  {
    if( ppa_blend->pushImageBlendAlpha(&ppa_fgsprite, ialpha, &ppa_bgsprite, 0xff-ialpha) ) {
      ppa_dstsprite.pushSprite(&tft, tft.width()/2-ppa_fgsprite.width()/2, 0);
    }
  }

}
