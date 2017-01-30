// This is a mash-up of the Due show() code + insights from Michael Miller's
// ESP8266 work for the NeoPixelBus library: github.com/Makuna/NeoPixelBus
// Needs to be a separate .c file to enforce ICACHE_RAM_ATTR execution.

#include "esp_neopixel.h"
#include "fw/src/mgos_console.h"
#include <stdio.h>

static uint32_t _getCycleCount(void) __attribute__((always_inline));
static inline uint32_t _getCycleCount(void)
{
  uint32_t ccount;
  __asm__ __volatile__("rsr %0,ccount"
                       : "=a"(ccount));
  return ccount;
}

void espShow(
    uint8_t pin, uint8_t *pixels, uint32_t numBytes, boolean is800KHz)
{

#define CYCLES_800_T0H (F_CPU / 2500000) // 0.4us
#define CYCLES_800_T1H (F_CPU / 1250000) // 0.8us
#define CYCLES_800 (F_CPU / 800000)      // 1.25us per bit
#define CYCLES_400_T0H (F_CPU / 2000000) // 0.5uS
#define CYCLES_400_T1H (F_CPU / 833333)  // 1.2us
#define CYCLES_400 (F_CPU / 400000)      // 2.5us per bit

  uint8_t *p, *end, pix, mask;
  uint32_t t, time0, time1, period, c, startTime, pinMask;

  pinMask = _BV(pin);
  p = pixels;
  end = p + numBytes;
  pix = *p++;
  mask = 0x80;
  startTime = 0;

#ifdef NEO_KHZ400
  if (is800KHz)
  {
#endif
    time0 = CYCLES_800_T0H;
    time1 = CYCLES_800_T1H;
    period = CYCLES_800;
#ifdef NEO_KHZ400
  }
  else
  { // 400 KHz bitstream
    time0 = CYCLES_400_T0H;
    time1 = CYCLES_400_T1H;
    period = CYCLES_400;
  }
#endif

  for (t = time0;; t = time0)
  {
    if (pix & mask)
      t = time1; // Bit high duration
    while (((c = _getCycleCount()) - startTime) < period)
      ;                                             // Wait for bit start
    GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, pinMask); // Set high
    startTime = c;                                  // Save start time
    while (((c = _getCycleCount()) - startTime) < t)
      ;                                             // Wait high duration
    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pinMask); // Set low
    if (!(mask >>= 1))
    { // Next bit/byte
      if (p >= end)
        break;
      pix = *p++;
      mask = 0x80;
    }
  }
  while ((_getCycleCount() - startTime) < period)
    ; // Wait for last bit
}

void Adafruit_NeoPixel__show(Adafruit_NeoPixel *this)
{

  if (!this->pixels)
    return;

  // Data latch = 50+ microsecond pause in the output stream.  Rather than
  // put a delay at the end of the function, the ending time is noted and
  // the function will simply hold off (if needed) on issuing the
  // subsequent round of data until the latch time has elapsed.  This
  // allows the mainline code to start generating the next frame of data
  // rather than stalling for the latch.
  while (!Adafruit_NeoPixel____inline__canShow(this))
    ;
  // endTime is a private member (rather than global var) so that mutliple
  // instances on different pins can be quickly issued in succession (each
  // instance doesn't delay the next).

  // In order to make this code runtime-configurable to work with any pin,
  // SBI/CBI instructions are eschewed in favor of full PORT writes via the
  // OUT or ST instructions.  It relies on two facts: that peripheral
  // functions (such as PWM) take precedence on output pins, so our PORT-
  // wide writes won't interfere, and that interrupts are globally disabled
  // while data is being issued to the LEDs, so no other code will be
  // accessing the PORT.  The code takes an initial 'snapshot' of the PORT
  // state, computes 'pin high' and 'pin low' values, and writes these back
  // to the PORT register as needed.

  noInterrupts(); // Need 100% focus on instruction timing

  // ESP8266 ----------------------------------------------------------------

  // ESP8266 show() is external to enforce ICACHE_RAM_ATTR execution
  espShow(this->pin, this->pixels, this->numBytes, this->is800KHz);

  // END ARCHITECTURE SELECT ------------------------------------------------

  interrupts();

  this->endTime = micros(); // Save EOD time for latch on next call
}

bool Adafruit_NeoPixel____inline__canShow(Adafruit_NeoPixel *this)
{
  return (micros() - this->endTime) >= 50L;
}
