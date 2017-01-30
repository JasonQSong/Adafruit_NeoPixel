/*-------------------------------------------------------------------------
  Arduino library to control a wide variety of WS2811- and WS2812-based RGB
  LED devices such as Adafruit FLORA RGB Smart Pixels and NeoPixel strips.
  Currently handles 400 and 800 KHz bitstreams on 8, 12 and 16 MHz ATmega
  MCUs, with LEDs wired for various color orders.  Handles most output pins
  (possible exception with upper PORT registers on the Arduino Mega).

  Written by Phil Burgess / Paint Your Dragon for Adafruit Industries,
  contributions by PJRC, Michael Miller and other members of the open
  source community.

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing products
  from Adafruit!

  -------------------------------------------------------------------------
  This file is part of the Adafruit NeoPixel library.

  NeoPixel is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, either version 3 of
  the License, or (at your option) any later version.

  NeoPixel is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with NeoPixel.  If not, see
  <http://www.gnu.org/licenses/>.
  -------------------------------------------------------------------------*/

#include "Adafruit_NeoPixel.h"

// Constructor when length, pin and type are known at compile-time:
Adafruit_NeoPixel *Adafruit_NeoPixel____init___n_p_t(uint16_t n, uint8_t p, neoPixelType t)
{
  Adafruit_NeoPixel *this = calloc(1, sizeof(Adafruit_NeoPixel));
  this->begun = false;
  this->brightness = 0;
  this->pixels = NULL;
  this->endTime = 0;
  Adafruit_NeoPixel__updateType_t(this, t);
  Adafruit_NeoPixel__updateLength_n(this, n);
  Adafruit_NeoPixel__setPin_p(this, p);
  this->showing = false;
  return this;
}

// via Michael Vogt/neophob: empty constructor is used when strand length
// isn't known at compile-time; situations where program config might be
// read from internal flash memory or an SD card, or arrive via serial
// command.  If using this constructor, MUST follow up with updateType(),
// updateLength(), etc. to establish the strand type, length and pin number!
Adafruit_NeoPixel *Adafruit_NeoPixel____init__()
{
  Adafruit_NeoPixel *this = calloc(1, sizeof(Adafruit_NeoPixel));
#ifdef NEO_KHZ400
  this->is800KHz = true;
#endif
  this->begun = false;
  this->numLEDs = 0;
  this->numBytes = 0;
  this->pin = -1;
  this->brightness = 0;
  this->pixels = NULL;
  this->rOffset = 1;
  this->gOffset = 0;
  this->bOffset = 2;
  this->wOffset = 1;
  this->endTime = 0;
  this->showing = false;
  return this;
}

void Adafruit_NeoPixel____del__(Adafruit_NeoPixel *this)
{
  if (this->pixels)
    free(this->pixels);
  if (this->pin >= 0)
    pinMode(this->pin, INPUT);
}

void Adafruit_NeoPixel__begin(Adafruit_NeoPixel *this)
{
  if (this->pin >= 0)
  {
    pinMode(this->pin, OUTPUT);
    digitalWrite(this->pin, LOW);
  }
  this->begun = true;
}

void Adafruit_NeoPixel__updateLength_n(Adafruit_NeoPixel *this, uint16_t n)
{
  if (this->pixels)
    free(this->pixels); // Free existing data (if any)

  // Allocate new data -- note: ALL PIXELS ARE CLEARED
  this->numBytes = n * ((this->wOffset == this->rOffset) ? 3 : 4);
  if ((this->pixels = (uint8_t *)malloc(this->numBytes)))
  {
    memset(this->pixels, 0, this->numBytes);
    this->numLEDs = n;
  }
  else
  {
    this->numLEDs = this->numBytes = 0;
  }
}

void Adafruit_NeoPixel__updateType_t(Adafruit_NeoPixel *this, neoPixelType t)
{
  boolean oldThreeBytesPerPixel = (this->wOffset == this->rOffset); // false if RGBW

  this->wOffset = (t >> 6) & 0b11; // See notes in header file
  this->rOffset = (t >> 4) & 0b11; // regarding R/G/B/W offsets
  this->gOffset = (t >> 2) & 0b11;
  this->bOffset = t & 0b11;
#ifdef NEO_KHZ400
  this->is800KHz = (t < 256); // 400 KHz flag is 1<<8
#endif

  // If bytes-per-pixel has changed (and pixel data was previously
  // allocated), re-allocate to new size.  Will clear any data.
  if (this->pixels)
  {
    boolean newThreeBytesPerPixel = (this->wOffset == this->rOffset);
    if (newThreeBytesPerPixel != oldThreeBytesPerPixel)
      Adafruit_NeoPixel__updateLength_n(this, this->numLEDs);
  }
}

// void Adafruit_NeoPixel__show(Adafruit_NeoPixel *this);

// Set the output pin number
void Adafruit_NeoPixel__setPin_p(Adafruit_NeoPixel *this, uint8_t p)
{
  if (this->begun && (this->pin >= 0))
    pinMode(this->pin, INPUT);
  this->pin = p;
  if (this->begun)
  {
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);
  }
#ifdef __AVR__
  port = portOutputRegister(digitalPinToPort(p));
  this->pinMask = digitalPinToBitMask(p);
#endif
}

// Set pixel color from separate R,G,B components:
void Adafruit_NeoPixel__setPixelColor_n_r_g_b(Adafruit_NeoPixel *this,
                                              uint16_t n, uint8_t r, uint8_t g, uint8_t b)
{

  if (n < this->numLEDs)
  {
    if (this->brightness)
    { // See notes in setBrightness()
      r = (r * this->brightness) >> 8;
      g = (g * this->brightness) >> 8;
      b = (b * this->brightness) >> 8;
    }
    uint8_t *p;
    if (this->wOffset == this->rOffset)
    {                             // Is an RGB-type strip
      p = &(this->pixels[n * 3]); // 3 bytes per pixel
    }
    else
    {                             // Is a WRGB-type strip
      p = &(this->pixels[n * 4]); // 4 bytes per pixel
      p[this->wOffset] = 0;       // But only R,G,B passed -- set W to 0
    }
    p[this->rOffset] = r; // R,G,B always stored
    p[this->gOffset] = g;
    p[this->bOffset] = b;
  }
}

void Adafruit_NeoPixel__setPixelColor_n_r_g_b_w(Adafruit_NeoPixel *this,
                                                uint16_t n, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{

  if (n < this->numLEDs)
  {
    if (this->brightness)
    { // See notes in setBrightness()
      r = (r * this->brightness) >> 8;
      g = (g * this->brightness) >> 8;
      b = (b * this->brightness) >> 8;
      w = (w * this->brightness) >> 8;
    }
    uint8_t *p;
    if (this->wOffset == this->rOffset)
    {                             // Is an RGB-type strip
      p = &(this->pixels[n * 3]); // 3 bytes per pixel (ignore W)
    }
    else
    {                             // Is a WRGB-type strip
      p = &(this->pixels[n * 4]); // 4 bytes per pixel
      p[this->wOffset] = w;       // Store W
    }
    p[this->rOffset] = r; // Store R,G,B
    p[this->gOffset] = g;
    p[this->bOffset] = b;
  }
}

// Set pixel color from 'packed' 32-bit RGB color:
void Adafruit_NeoPixel__setPixelColor_n_c(Adafruit_NeoPixel *this, uint16_t n, uint32_t c)
{
  if (n < this->numLEDs)
  {
    uint8_t *p,
        r = (uint8_t)(c >> 16),
        g = (uint8_t)(c >> 8),
        b = (uint8_t)c;
    if (this->brightness)
    { // See notes in setBrightness()
      r = (r * this->brightness) >> 8;
      g = (g * this->brightness) >> 8;
      b = (b * this->brightness) >> 8;
    }
    if (this->wOffset == this->rOffset)
    {
      p = &(this->pixels[n * 3]);
    }
    else
    {
      p = &(this->pixels[n * 4]);
      uint8_t w = (uint8_t)(c >> 24);
      p[this->wOffset] = this->brightness ? ((w * this->brightness) >> 8) : w;
    }
    p[this->rOffset] = r;
    p[this->gOffset] = g;
    p[this->bOffset] = b;
  }
}

// Convert separate R,G,B into packed 32-bit RGB color.
// Packed format is always RGB, regardless of LED strand color order.
uint32_t Adafruit_NeoPixel____static__Color_r_g_b(uint8_t r, uint8_t g, uint8_t b)
{
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// Convert separate R,G,B,W into packed 32-bit WRGB color.
// Packed format is always WRGB, regardless of LED strand color order.
uint32_t Adafruit_NeoPixel____static__Color_r_g_b_w(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
  return ((uint32_t)w << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// Query color from previously-set pixel (returns packed 32-bit RGB value)
uint32_t Adafruit_NeoPixel__getPixelColor_n(Adafruit_NeoPixel *this, uint16_t n)
{
  if (n >= this->numLEDs)
    return 0; // Out of bounds, return no color.

  uint8_t *p;

  if (this->wOffset == this->rOffset)
  { // Is RGB-type device
    p = &(this->pixels[n * 3]);
    if (this->brightness)
    {
      // Stored color was decimated by setBrightness().  Returned value
      // attempts to scale back to an approximation of the original 24-bit
      // value used when setting the pixel color, but there will always be
      // some error -- those bits are simply gone.  Issue is most
      // pronounced at low brightness levels.
      return (((uint32_t)(p[this->rOffset] << 8) / this->brightness) << 16) |
             (((uint32_t)(p[this->gOffset] << 8) / this->brightness) << 8) |
             ((uint32_t)(p[this->bOffset] << 8) / this->brightness);
    }
    else
    {
      // No brightness adjustment has been made -- return 'raw' color
      return ((uint32_t)p[this->rOffset] << 16) |
             ((uint32_t)p[this->gOffset] << 8) |
             (uint32_t)p[this->bOffset];
    }
  }
  else
  { // Is RGBW-type device
    p = &(this->pixels[n * 4]);
    if (this->brightness)
    { // Return scaled color
      return (((uint32_t)(p[this->wOffset] << 8) / this->brightness) << 24) |
             (((uint32_t)(p[this->rOffset] << 8) / this->brightness) << 16) |
             (((uint32_t)(p[this->gOffset] << 8) / this->brightness) << 8) |
             ((uint32_t)(p[this->bOffset] << 8) / this->brightness);
    }
    else
    { // Return raw color
      return ((uint32_t)p[this->wOffset] << 24) |
             ((uint32_t)p[this->rOffset] << 16) |
             ((uint32_t)p[this->gOffset] << 8) |
             (uint32_t)p[this->bOffset];
    }
  }
}

// Returns pointer to pixels[] array.  Pixel data is stored in device-
// native format and is not translated here.  Application will need to be
// aware of specific pixel data format and handle colors appropriately.
uint8_t *Adafruit_NeoPixel__getPixels(Adafruit_NeoPixel *this)
{
  return this->pixels;
}

uint16_t Adafruit_NeoPixel__numPixels(Adafruit_NeoPixel *this)
{
  return this->numLEDs;
}

// Adjust output brightness; 0=darkest (off), 255=brightest.  This does
// NOT immediately affect what's currently displayed on the LEDs.  The
// next call to show() will refresh the LEDs at this level.  However,
// this process is potentially "lossy," especially when increasing
// brightness.  The tight timing in the WS2811/WS2812 code means there
// aren't enough free cycles to perform this scaling on the fly as data
// is issued.  So we make a pass through the existing color data in RAM
// and scale it (subsequent graphics commands also work at this
// brightness level).  If there's a significant step up in brightness,
// the limited number of steps (quantization) in the old data will be
// quite visible in the re-scaled version.  For a non-destructive
// change, you'll need to re-render the full strip data.  C'est la vie.
void Adafruit_NeoPixel__setBrightness(Adafruit_NeoPixel *this, uint8_t b)
{
  // Stored brightness value is different than what's passed.
  // This simplifies the actual scaling math later, allowing a fast
  // 8x8-bit multiply and taking the MSB.  'brightness' is a uint8_t,
  // adding 1 here may (intentionally) roll over...so 0 = max brightness
  // (color values are interpreted literally; no scaling), 1 = min
  // brightness (off), 255 = just below max brightness.
  uint8_t newBrightness = b + 1;
  if (newBrightness != this->brightness)
  { // Compare against prior value
    // Brightness has changed -- re-scale existing data in RAM
    uint8_t c,
        *ptr = this->pixels,
        oldBrightness = this->brightness - 1; // De-wrap old brightness value
    uint16_t scale;
    if (oldBrightness == 0)
      scale = 0; // Avoid /0
    else if (b == 255)
      scale = 65535 / oldBrightness;
    else
      scale = (((uint16_t)newBrightness << 8) - 1) / oldBrightness;
    for (uint16_t i = 0; i < this->numBytes; i++)
    {
      c = *ptr;
      *ptr++ = (c * scale) >> 8;
    }
    this->brightness = newBrightness;
  }
}

//Return the brightness value
uint8_t Adafruit_NeoPixel__getBrightness(Adafruit_NeoPixel *this)
{
  return this->brightness - 1;
}

void Adafruit_NeoPixel__clear(Adafruit_NeoPixel *this)
{
  memset(this->pixels, 0, this->numBytes);
}

int8_t Adafruit_NeoPixel__getPin(Adafruit_NeoPixel *this)
{
  return this->pin;
}
