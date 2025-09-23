
#include <M5Unified.hpp>
#include <LGFX_PPA.hpp>

#define tft M5.Display

lgfx::PPA_Sprite ppa_sprite;
lgfx::PPAFill *ppa_fill;


void setup()
{

  M5.begin();

  Serial.println("Hello PPA - Fill example");

  ppa_fill = new lgfx::PPAFill(&tft);

  ppa_fill->fillRect(0, 0, tft.width(), tft.height(), 0xffffffu);

}



void loop()
{
  static uint32_t dw = tft.width();
  static uint32_t dh = tft.height();

  if(ppa_fill->available())
  {

    lgfx::argb8888_t color = random();
    uint32_t xs=random()%dw, xe=random()%dw, ys=random()%dh, ye=random()%dh;

    if( xs==xe || ys==ye )
      return;

    if( xs>xe )
      std::swap(xs, xe);
    if( ys>ye )
      std::swap(ys, ye);

    if(! ppa_fill->fillRect(xs, ys, xe-xs, ye-ys, color) ) {
      Serial.println("ppa fill failed");
    }
  }


}
