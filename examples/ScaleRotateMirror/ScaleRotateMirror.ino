
#include <M5Unified.hpp>
#include <LGFX_PPA.hpp>

#define tft M5.Display

lgfx::PPA_Sprite ppa_sprite;
lgfx::PPASrm *ppa_srm;

uint32_t src_x=0, src_y=0;
float rotation=0; // only multiples of 90 e.g. 0/90/180/270/360/etc...
float scale_x=3, scale_y=3;
uint32_t src_w=0, src_h=0;

void setup()
{

  M5.begin();

  Serial.println("Hello PPA - Scale Rotate Mirror example");

  size_t boxWidth     = 128;
  size_t boxHeight    = 192;
  size_t borderWidth  = boxWidth/16;
  size_t borderHeight = boxHeight/16;

  ppa_sprite.createSprite(boxWidth, boxHeight); // 16bits colors forced
  ppa_sprite.fillGradientRect(borderWidth, borderHeight, ppa_sprite.width()-borderWidth*2, ppa_sprite.height()-borderHeight*2, 0xff0080, 0x00ff80, lgfx::RADIAL);
  ppa_sprite.setTextSize(8);
  ppa_sprite.setTextColor(TFT_WHITE);
  ppa_sprite.setTextDatum(MC_DATUM);;
  ppa_sprite.drawString("?", ppa_sprite.width()/2, ppa_sprite.height()/2);

  src_w = ppa_sprite.width();
  src_h = ppa_sprite.height();

  ppa_srm = new lgfx::PPASrm(&tft, false);

  // 1) test pushSRM() rotation only

  ppa_sprite.setRotation(0);
  ppa_srm->pushSRM(&ppa_sprite, 0, 0);

  ppa_sprite.setRotation(2);
  ppa_srm->pushSRM(&ppa_sprite, boxWidth, 0);

  ppa_sprite.setRotation(1);
  ppa_srm->pushSRM(&ppa_sprite, boxWidth*2, 0);

  ppa_sprite.setRotation(3);
  ppa_srm->pushSRM(&ppa_sprite, boxWidth*2+boxHeight, 0);

  // 2) test pushSRM() mirror only

  // reset rotation
  ppa_sprite.setRotation(0);
  ppa_srm->setMirror(true, false);
  ppa_srm->pushSRM(&ppa_sprite, 0,          boxHeight, 1.0, 1.0);

  ppa_srm->setMirror(false, true);
  ppa_srm->pushSRM(&ppa_sprite, boxWidth,   boxHeight, 1.0, 1.0);

  // reset mirror
  ppa_srm->setMirror(false, false);



}


float f = 0, fstep=M_PI/160.0;

void loop()
{
  if(ppa_srm->available())
  {
    float s = sin(f);
    float c = cos(f*.89);
    float sx = scale_x/2 + (s*scale_x/3);
    float sy = scale_y/2 + (c*scale_y/3);
    uint32_t dst_w = src_w*sx,
             dst_h = src_h*sy;
    uint32_t dst_x = tft.width()/2-dst_w/2,
             dst_y = tft.height()/2-dst_h/2;

    if( ppa_srm->pushSRM(&ppa_sprite, dst_x, dst_y, sx, sy ) ) {
      f += fstep;
    }
  }

}






