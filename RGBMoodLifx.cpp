#include "RGBMoodLifx.h"


// Dim curve
// Used to make 'dimming' look more natural. 
uint8_t dc[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

uint16_t red_factor = 4;
uint16_t green_factor = 8;
uint16_t blue_factor = 25;
uint16_t white_factor = 400;

// Constructor. Start with leds off.
RGBMoodLifx::RGBMoodLifx(uint8_t rp, uint8_t gp, uint8_t bp, uint8_t wp, uint16_t kelvin)
{
  mode_ = FIX_MODE; // Stand still
  pins_[0] = rp;
  pins_[1] = gp;
  pins_[2] = bp;
  pins_[3] = wp;
  led_kelvin_ = kelvin;
  current_RGBW_color_[0] = 0;
  current_RGBW_color_[1] = 0;
  current_RGBW_color_[2] = 0;
  current_RGBW_color_[3] = 0;
  current_HSB_color_[0] = 0;
  current_HSB_color_[1] = 0;
  current_HSB_color_[2] = 0;
  fading_max_steps_ = 200;
  fading_step_time_ = 50;
  holding_color_ = 1000;
  fading_ = false;
  last_update_ = millis();
}

/*
Change instantly the LED colors.
@param h The hue (0..65535) (Will be % 360)
@param s The saturation value (0..255)
@param b The brightness (0..255)
*/
void RGBMoodLifx::setHSB(uint16_t h, uint16_t s, uint16_t b) {
  current_HSB_color_[0] = constrain(h % 360, 0, 360);
  current_HSB_color_[1] = constrain(s, 0, 255);
  current_HSB_color_[2] = constrain(b, 0, 255);
  hsb2rgbw(current_HSB_color_[0], current_HSB_color_[1], current_HSB_color_[2], current_RGBW_color_[0], current_RGBW_color_[1], current_RGBW_color_[2], current_RGBW_color_[3]);
  fading_ = false;
}

/*
Change instantly the LED colors.
@param r The red (0..255)
@param g The green (0..255)
@param b The blue (0..255)
*/
void RGBMoodLifx::setRGBW(uint16_t r, uint16_t g, uint16_t b, uint16_t w) {
  current_RGBW_color_[0] = constrain(r, 0, 255);
  current_RGBW_color_[1] = constrain(g, 0, 255);
  current_RGBW_color_[2] = constrain(b, 0, 255);
  current_RGBW_color_[3] = constrain(w, 0, 255);
  fading_ = false;
}

void RGBMoodLifx::setRGBW(uint32_t color) {
  setRGBW((color & 0xFF000000) >> 24, (color & 0x00FF0000) >> 16, (color & 0x0000FF00) >> 8, color & 0x000000FF);
}

/*
Fade from current color to the one provided.
@param h The hue (0..65535) (Will be % 360)
@param s The saturation value (0..255)
@param b The brightness (0..255)
@param shortest Hue takes the shortest path (going up or down)
*/
void RGBMoodLifx::fadeHSB(uint16_t h, uint16_t s, uint16_t b, bool shortest) {
  initial_color_[0] = current_HSB_color_[0];
  initial_color_[1] = current_HSB_color_[1];
  initial_color_[2] = current_HSB_color_[2];
  if (shortest) {
    h = h % 360;
    // We take the shortest way! (0 == 360)
    // Example, if we fade from 10 to h=350, better fade from 370 to h=350.
    //          if we fade from 350 to h=10, better fade from 350 to h=370.
    // 10 -> 350
    if (initial_color_[0] < h) {
      if (h - initial_color_[0] > (initial_color_[0] + 360) - h)
        initial_color_[0] += 360;
    }
    else if (initial_color_[0] > h) { // 350 -> 10
      if (initial_color_[0] - h > (h + 360) - initial_color_[0])
        h += 360;
    }
  }
  target_color_[0] = h;
  target_color_[1] = s;
  target_color_[2] = b;
  fading_ = true;
  fading_step_ = 0;
  fading_in_hsb_ = true;
}

/*
Fade from current color to the one provided.
@param r The red (0..255)
@param g The green (0..255)
@param b The blue (0..255)
*/
void RGBMoodLifx::fadeRGBW(uint16_t r, uint16_t g, uint16_t b, uint16_t w) {
  initial_color_[0] = current_RGBW_color_[0];
  initial_color_[1] = current_RGBW_color_[1];
  initial_color_[2] = current_RGBW_color_[2];
  initial_color_[3] = current_RGBW_color_[3];
  target_color_[0] = r;
  target_color_[1] = g;
  target_color_[2] = b;
  target_color_[3] = w;
  fading_ = true;
  fading_step_ = 0;
  fading_in_hsb_ = false;
}

void RGBMoodLifx::fadeRGBW(uint32_t color) {
  fadeRGBW((color & 0xFF000000) >> 24, (color & 0x00FF0000) >> 16, (color & 0x0000FF00) >> 8, color & 0x000000FF);
}

/*
Fade from current color to the kelvin color .
@param kelvin (2500..9000)
*/
void RGBMoodLifx::fadeKelvin(uint16_t kelvin, uint16_t brightness) {
  initial_color_[0] = current_RGBW_color_[0];
  initial_color_[1] = current_RGBW_color_[1];
  initial_color_[2] = current_RGBW_color_[2];
  initial_color_[3] = current_RGBW_color_[3];
  //setTargetKelvinValue(kelvin, brightness);
  kelvinToRGBW(kelvin, brightness, target_color_[0], target_color_[1], target_color_[2], target_color_[3]);
  fading_ = true;
  fading_step_ = 0;
  fading_in_hsb_ = false;
}

/*
Set the current color to the kelvin color .
@param kelvin (2500..9000)
*/
void RGBMoodLifx::setKelvin(uint16_t kelvin, uint16_t brightness) {
  setKelvinValue(kelvin, brightness);
  //setTargetKelvinValue(kelvin, brightness);
  kelvinToRGBW(kelvin, brightness, current_RGBW_color_[0], current_RGBW_color_[1], current_RGBW_color_[2], current_RGBW_color_[3]);
  fading_ = false;
}

/*
This function needs to be called in the loop function.
*/
void RGBMoodLifx::tick() {
  unsigned long current_millis = millis();
  if (fading_) {
    // Enough time since the last step ?
    if (current_millis - last_update_ >= fading_step_time_) {
      fading_step_++;
      fade();
      if (fading_step_ >= fading_max_steps_) {
        fading_ = false;
        if (fading_in_hsb_) {
          current_HSB_color_[0] = target_color_[0] % 360;
          current_HSB_color_[1] = target_color_[1];
          current_HSB_color_[2] = target_color_[2];
        }
      }
      last_update_ = current_millis;
    }
  }
  else if (mode_ != FIX_MODE) {
    // We are not fading.
    // If mode_ == 0, we do nothing.
    if (current_millis - last_update_ >= holding_color_) {
      last_update_ = current_millis;
      switch(mode_) {
        case RANDOM_HUE_MODE:
          fadeHSB(random(0, 360), current_HSB_color_[1], current_HSB_color_[2]);
          break;
        case RAINBOW_HUE_MODE:
          fadeHSB(360, current_HSB_color_[1], current_HSB_color_[2], false);
          break;
        case RED_MODE:
          fadeHSB(random(335, 400), random(190, 255), random(120, 255));
          break;
        case BLUE_MODE:
          fadeHSB(random(160, 275), random(190, 255), random(120, 255));
          break;
        case GREEN_MODE:
          fadeHSB(random(72, 160), random(190, 255), random(120, 255));
          break;
        case FIRE_MODE:
          setHSB(random(345, 435), random(190, 255), random(120,255));
          holding_color_ = random(10, 500);
          break;
      }
    }
  }
  //if (pins_[0] > 0) {
  if (pins_[0] >= 0) { // this is the modification for LIFX - allows 0 to be written for each pin to power off the LED
    analogWrite(pins_[0], current_RGBW_color_[0]);
    analogWrite(pins_[1], current_RGBW_color_[1]);
    analogWrite(pins_[2], current_RGBW_color_[2]);
    analogWrite(pins_[3], current_RGBW_color_[3]);
  }
}

/*
Convert a HSB color to RGB
This function is used internally but may be used by the end user too. (public).
@param h The hue (0..65535) (Will be % 360)
@param s The saturation value (0..255)
@param b The brightness (0..255)
*/
void RGBMoodLifx::hsb2rgb(uint16_t hue, uint16_t sat, uint16_t val, uint16_t& red, uint16_t& green, uint16_t& blue) {
  val = dc[val];
  sat = 255-dc[255-sat];
  hue = hue % 360;

  int r;
  int g;
  int b;
  int base;

  if (sat == 0) { // Acromatic color (gray). Hue doesn't mind.
    red   = val;
    green = val;
    blue  = val;
  } else  {
    base = ((255 - sat) * val)>>8;
    switch(hue/60) {
      case 0:
		    r = val;
        g = (((val-base)*hue)/60)+base;
        b = base;
        break;
	
      case 1:
        r = (((val-base)*(60-(hue%60)))/60)+base;
        g = val;
        b = base;
        break;
	
      case 2:
        r = base;
        g = val;
        b = (((val-base)*(hue%60))/60)+base;
        break;

      case 3:
        r = base;
        g = (((val-base)*(60-(hue%60)))/60)+base;
        b = val;
        break;
	
      case 4:
        r = (((val-base)*(hue%60))/60)+base;
        g = base;
        b = val;
        break;
	
      case 5:
        r = val;
        g = base;
        b = (((val-base)*(60-(hue%60)))/60)+base;
        break;
    }  
    red   = r;
    green = g;
    blue  = b; 
  }
}

/*
Convert a HSB color to RGBW
This function is used internally but may be used by the end user too. (public).
@param h The hue (0..65535) (Will be % 360)
@param s The saturation value (0..255)
@param b The brightness (0..255)
*/
void RGBMoodLifx::hsb2rgbw(uint16_t hue, uint16_t sat, uint16_t val, uint16_t& red, uint16_t& green, uint16_t& blue, uint16_t& white) {
  val = dc[val];
  //sat = 255-dc[255-sat];
  hue = hue % 360;

  int r;
  int g;
  int b;

  if (sat == 0) { // Acromatic color (gray). Hue doesn't mind.
    red   = val;
    green = val;
    blue  = val;
    white = val;
  } else  {
    switch(hue/60) {
      case 0: // hue 0 t.e.m 59
        r = val-(((val)*hue)/120);
        g = (((val)*hue)/120);
        b = 0;
        break;
  
      case 1: // hue 60 t.e.m 119
        r = (((val)*(60-(hue%60)))/120);
        g = val-(((val)*(60-(hue%60)))/120);
        b = 0;
        break;
  
      case 2: // hue 120 t.e.m 179
        r = 0;
        g = val-(((val)*(hue%60))/120);
        b = (((val)*(hue%60))/120);
        break;

      case 3: // hue 180 t.e.m 239
        r = 0;
        g = (((val)*(60-(hue%60)))/120);
        b = val-(((val)*(60-(hue%60)))/120);
        break;
  
      case 4: // hue 240 t.e.m 359
        r = (((val)*(hue%60))/120);
        g = 0;
        b = val-(((val)*(hue%60))/120);
        break;
  
      case 5:
        r = val-(((val)*(60-(hue%60)))/120);
        g = 0;
        b = (((val)*(60-(hue%60)))/120);
        break;
    }

    red   = r;
    green = g;
    blue = b;
    white = ((255-sat)*val) >> 8;
    
    
    Serial.print("HSV: ");
    Serial.print(hue);
    Serial.print(", ");
    Serial.print(sat);
    Serial.print(", ");
    Serial.println(val);

    Serial.print("RGBW: ");
    Serial.print(red);
    Serial.print(", ");
    Serial.print(green);
    Serial.print(", ");
    Serial.print(blue);
    Serial.print(", ");
    Serial.println(white);
  }
}

// Convert Kelvin to RGBW value
void RGBMoodLifx::kelvinToRGBW(uint16_t kelvin, uint16_t brightness, uint16_t& red, uint16_t& green, uint16_t& blue, uint16_t& white) {
  brightness = dc[brightness];
  long temperature = kelvin / 100;

  if(temperature <= 66) {
    red = 255;
  } 
  else {
    red = temperature - 60;
    red = 329.698727446 * pow(red, -0.1332047592);
    if(red < 0) red = 0;
    if(red > 255) red = 255;
  }

  if(temperature <= 66) {
    green = temperature;
    green = 99.4708025861 * log(green) - 161.1195681661;
    if(green < 0) green = 0;
    if(green > 255) green = 255;
  } 
  else {
    green = temperature - 60;
    green = 288.1221695283 * pow(green, -0.0755148492);
    if(green < 0) green = 0;
    if(green > 255) green = 255;
  }

  if(temperature >= 66) {
    blue = 255;
  } 
  else {
    if(temperature <= 19) {
      blue = 0;
    } 
    else {
      blue = temperature - 10;
      blue = 138.5177312231 * log(blue) - 305.0447927307;
      if(blue < 0) blue = 0;
      if(blue > 255) blue = 255;
    }
  }

  white = 255;

  // adjust brightness
  red = map(red, 0, 255, 0, brightness);
  green = map(green, 0, 255, 0, brightness);
  blue = map(blue, 0, 255, 0, brightness);
  white = map(white, 0, 255, 0, brightness);
}

/*  Private functions
------------------------------------------------------------ */

/*
This function is used internaly to do the fading between colors.
*/
void RGBMoodLifx::fade()
{ 
  if (fading_in_hsb_) {
    current_HSB_color_[0] = (uint16_t)(initial_color_[0] - (fading_step_*((initial_color_[0]-(float)target_color_[0])/fading_max_steps_)));
    current_HSB_color_[1] = (uint16_t)(initial_color_[1] - (fading_step_*((initial_color_[1]-(float)target_color_[1])/fading_max_steps_)));
    current_HSB_color_[2] = (uint16_t)(initial_color_[2] - (fading_step_*((initial_color_[2]-(float)target_color_[2])/fading_max_steps_)));
    hsb2rgbw(current_HSB_color_[0], current_HSB_color_[1], current_HSB_color_[2], current_RGBW_color_[0], current_RGBW_color_[1], current_RGBW_color_[2], current_RGBW_color_[3]);
  }
  else {
    current_RGBW_color_[0] = (uint16_t)(initial_color_[0] - (fading_step_*((initial_color_[0]-(float)target_color_[0])/fading_max_steps_)));
    current_RGBW_color_[1] = (uint16_t)(initial_color_[1] - (fading_step_*((initial_color_[1]-(float)target_color_[1])/fading_max_steps_)));
    current_RGBW_color_[2] = (uint16_t)(initial_color_[2] - (fading_step_*((initial_color_[2]-(float)target_color_[2])/fading_max_steps_)));
    current_RGBW_color_[3] = (uint16_t)(initial_color_[3] - (fading_step_*((initial_color_[3]-(float)target_color_[3])/fading_max_steps_)));
  }
}

void RGBMoodLifx::setKelvinValue(uint16_t kelvin, uint16_t brightness)
{
  brightness = 255-dc[brightness];
  int change = (kelvin - led_kelvin_);
  
  if(change < 0)
  {
    current_RGBW_color_[0] = (uint16_t)(-change/(red_factor*brightness));
    current_RGBW_color_[1] = (uint16_t)(-change/(green_factor*brightness));
    current_RGBW_color_[2] = 0;
    current_RGBW_color_[3] = (uint16_t)((255 + (change/white_factor))/brightness);
  }
  else
  {
    current_RGBW_color_[0] = 0;
    current_RGBW_color_[1] = 0;
    current_RGBW_color_[2] = (uint16_t)(change/(blue_factor*brightness));
    current_RGBW_color_[3] = (uint16_t)((255 - (change/white_factor))/brightness);
  }
}

void RGBMoodLifx::setTargetKelvinValue(uint16_t kelvin, uint16_t brightness)
{
  int change = (kelvin - led_kelvin_);
  brightness = 255-dc[brightness];
  
  if(change < 0)
  {
    target_color_[0] = (uint16_t)(-change/(red_factor*brightness));
    target_color_[1] = (uint16_t)(-change/(green_factor*brightness));
    target_color_[2] = 0;
    target_color_[3] = (uint16_t)((255 + (change/white_factor))/brightness);
  }
  else
  {
    target_color_[0] = 0;
    target_color_[1] = 0;
    target_color_[2] = (uint16_t)(change/(blue_factor*brightness));
    target_color_[3] = (uint16_t)((255 - (change/white_factor))/brightness);
  }
}

