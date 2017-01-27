// This is a mash-up of the Due show() code + insights from Michael Miller's
// ESP8266 work for the NeoPixelBus library: github.com/Makuna/NeoPixelBus
// Needs to be a separate .c file to enforce ICACHE_RAM_ATTR execution.

#include "esp_neopixel.h"

#include <user_interface.h>
#include <c_types.h>
#include <eagle_soc.h>

static uint32_t _getCycleCount(void) __attribute__((always_inline));
static inline uint32_t _getCycleCount(void)
{
    uint32_t ccount;
    __asm__ __volatile__("rsr %0,ccount"
                         : "=a"(ccount));
    return ccount;
}

void ICACHE_FLASH_ATTR espShow(
    uint8_t pin, uint8_t *pixels, uint32_t numBytes, boolean is800KHz)
{

#define CYCLES_800_T0H (SYS_CPU_80MHZ * 1000000 / 2500000) // 0.4us
#define CYCLES_800_T1H (SYS_CPU_80MHZ * 1000000 / 1250000) // 0.8us
#define CYCLES_800 (SYS_CPU_80MHZ * 1000000 / 800000)      // 1.25us per bit
#define CYCLES_400_T0H (SYS_CPU_80MHZ * 1000000 / 2000000) // 0.5uS
#define CYCLES_400_T1H (SYS_CPU_80MHZ * 1000000 / 833333)  // 1.2us
#define CYCLES_400 (SYS_CPU_80MHZ * 1000000 / 400000)      // 2.5us per bit

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
            ;                                           // Wait for bit start
        GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, pinMask); // Set high
        startTime = c;                                  // Save start time
        while (((c = _getCycleCount()) - startTime) < t)
            ;                                           // Wait high duration
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

bool Adafruit_NeoPixel__canShow(Adafruit_NeoPixel *this)
{
    return (system_get_time() - this->endTime) >= 50L;
}

// void Adafruit_NeoPixel__show(Adafruit_NeoPixel *this) {

//   if(!this->pixels) return;

//   // Data latch = 50+ microsecond pause in the output stream.  Rather than
//   // put a delay at the end of the function, the ending time is noted and
//   // the function will simply hold off (if needed) on issuing the
//   // subsequent round of data until the latch time has elapsed.  This
//   // allows the mainline code to start generating the next frame of data
//   // rather than stalling for the latch.
//   while(!canShow());
//   // endTime is a private member (rather than global var) so that mutliple
//   // instances on different pins can be quickly issued in succession (each
//   // instance doesn't delay the next).

//   // In order to make this code runtime-configurable to work with any pin,
//   // SBI/CBI instructions are eschewed in favor of full PORT writes via the
//   // OUT or ST instructions.  It relies on two facts: that peripheral
//   // functions (such as PWM) take precedence on output pins, so our PORT-
//   // wide writes won't interfere, and that interrupts are globally disabled
//   // while data is being issued to the LEDs, so no other code will be
//   // accessing the PORT.  The code takes an initial 'snapshot' of the PORT
//   // state, computes 'pin high' and 'pin low' values, and writes these back
//   // to the PORT register as needed.

//   noInterrupts(); // Need 100% focus on instruction timing

// #ifdef __AVR__
// // AVR MCUs -- ATmega & ATtiny (no XMEGA) ---------------------------------

//   volatile uint16_t
//     i   = this->numBytes; // Loop counter
//   volatile uint8_t
//    *ptr = this->pixels,   // Pointer to next byte
//     b   = *ptr++,   // Current byte value
//     hi,             // PORT w/output bit set high
//     lo;             // PORT w/output bit set low

//   // Hand-tuned assembly code issues data to the LED drivers at a specific
//   // rate.  There's separate code for different CPU speeds (8, 12, 16 MHz)
//   // for both the WS2811 (400 KHz) and WS2812 (800 KHz) drivers.  The
//   // datastream timing for the LED drivers allows a little wiggle room each
//   // way (listed in the datasheets), so the conditions for compiling each
//   // case are set up for a range of frequencies rather than just the exact
//   // 8, 12 or 16 MHz values, permitting use with some close-but-not-spot-on
//   // devices (e.g. 16.5 MHz DigiSpark).  The ranges were arrived at based
//   // on the datasheet figures and have not been extensively tested outside
//   // the canonical 8/12/16 MHz speeds; there's no guarantee these will work
//   // close to the extremes (or possibly they could be pushed further).
//   // Keep in mind only one CPU speed case actually gets compiled; the
//   // resulting program isn't as massive as it might look from source here.

// // 8 MHz(ish) AVR ---------------------------------------------------------
// #if (F_CPU >= 7400000UL) && (F_CPU <= 9500000UL)

// #ifdef NEO_KHZ400 // 800 KHz check needed only if 400 KHz support enabled
//   if(is800KHz) {
// #endif

//     volatile uint8_t n1, n2 = 0;  // First, next bits out

//     // Squeezing an 800 KHz stream out of an 8 MHz chip requires code
//     // specific to each PORT register.

//     // 10 instruction clocks per bit: HHxxxxxLLL
//     // OUT instructions:              ^ ^    ^   (T=0,2,7)

//     // PORTD OUTPUT ----------------------------------------------------

// #if defined(PORTD)
//  #if defined(PORTB) || defined(PORTC) || defined(PORTF)
//     if(port == &PORTD) {
//  #endif // defined(PORTB/C/F)

//       hi = PORTD |  this->pinMask;
//       lo = PORTD & ~this->pinMask;
//       n1 = lo;
//       if(b & 0x80) n1 = hi;

//       // Dirty trick: RJMPs proceeding to the next instruction are used
//       // to delay two clock cycles in one instruction word (rather than
//       // using two NOPs).  This was necessary in order to squeeze the
//       // loop down to exactly 64 words -- the maximum possible for a
//       // relative branch.

//       asm volatile(
//        "headD:"                   "\n\t" // Clk  Pseudocode
//         // Bit 7:
//         "out  %[port] , %[hi]"    "\n\t" // 1    PORT = hi
//         "mov  %[n2]   , %[lo]"    "\n\t" // 1    n2   = lo
//         "out  %[port] , %[n1]"    "\n\t" // 1    PORT = n1
//         "rjmp .+0"                "\n\t" // 2    nop nop
//         "sbrc %[byte] , 6"        "\n\t" // 1-2  if(b & 0x40)
//          "mov %[n2]   , %[hi]"    "\n\t" // 0-1   n2 = hi
//         "out  %[port] , %[lo]"    "\n\t" // 1    PORT = lo
//         "rjmp .+0"                "\n\t" // 2    nop nop
//         // Bit 6:
//         "out  %[port] , %[hi]"    "\n\t" // 1    PORT = hi
//         "mov  %[n1]   , %[lo]"    "\n\t" // 1    n1   = lo
//         "out  %[port] , %[n2]"    "\n\t" // 1    PORT = n2
//         "rjmp .+0"                "\n\t" // 2    nop nop
//         "sbrc %[byte] , 5"        "\n\t" // 1-2  if(b & 0x20)
//          "mov %[n1]   , %[hi]"    "\n\t" // 0-1   n1 = hi
//         "out  %[port] , %[lo]"    "\n\t" // 1    PORT = lo
//         "rjmp .+0"                "\n\t" // 2    nop nop
//         // Bit 5:
//         "out  %[port] , %[hi]"    "\n\t" // 1    PORT = hi
//         "mov  %[n2]   , %[lo]"    "\n\t" // 1    n2   = lo
//         "out  %[port] , %[n1]"    "\n\t" // 1    PORT = n1
//         "rjmp .+0"                "\n\t" // 2    nop nop
//         "sbrc %[byte] , 4"        "\n\t" // 1-2  if(b & 0x10)
//          "mov %[n2]   , %[hi]"    "\n\t" // 0-1   n2 = hi
//         "out  %[port] , %[lo]"    "\n\t" // 1    PORT = lo
//         "rjmp .+0"                "\n\t" // 2    nop nop
//         // Bit 4:
//         "out  %[port] , %[hi]"    "\n\t" // 1    PORT = hi
//         "mov  %[n1]   , %[lo]"    "\n\t" // 1    n1   = lo
//         "out  %[port] , %[n2]"    "\n\t" // 1    PORT = n2
//         "rjmp .+0"                "\n\t" // 2    nop nop
//         "sbrc %[byte] , 3"        "\n\t" // 1-2  if(b & 0x08)
//          "mov %[n1]   , %[hi]"    "\n\t" // 0-1   n1 = hi
//         "out  %[port] , %[lo]"    "\n\t" // 1    PORT = lo
//         "rjmp .+0"                "\n\t" // 2    nop nop
//         // Bit 3:
//         "out  %[port] , %[hi]"    "\n\t" // 1    PORT = hi
//         "mov  %[n2]   , %[lo]"    "\n\t" // 1    n2   = lo
//         "out  %[port] , %[n1]"    "\n\t" // 1    PORT = n1
//         "rjmp .+0"                "\n\t" // 2    nop nop
//         "sbrc %[byte] , 2"        "\n\t" // 1-2  if(b & 0x04)
//          "mov %[n2]   , %[hi]"    "\n\t" // 0-1   n2 = hi
//         "out  %[port] , %[lo]"    "\n\t" // 1    PORT = lo
//         "rjmp .+0"                "\n\t" // 2    nop nop
//         // Bit 2:
//         "out  %[port] , %[hi]"    "\n\t" // 1    PORT = hi
//         "mov  %[n1]   , %[lo]"    "\n\t" // 1    n1   = lo
//         "out  %[port] , %[n2]"    "\n\t" // 1    PORT = n2
//         "rjmp .+0"                "\n\t" // 2    nop nop
//         "sbrc %[byte] , 1"        "\n\t" // 1-2  if(b & 0x02)
//          "mov %[n1]   , %[hi]"    "\n\t" // 0-1   n1 = hi
//         "out  %[port] , %[lo]"    "\n\t" // 1    PORT = lo
//         "rjmp .+0"                "\n\t" // 2    nop nop
//         // Bit 1:
//         "out  %[port] , %[hi]"    "\n\t" // 1    PORT = hi
//         "mov  %[n2]   , %[lo]"    "\n\t" // 1    n2   = lo
//         "out  %[port] , %[n1]"    "\n\t" // 1    PORT = n1
//         "rjmp .+0"                "\n\t" // 2    nop nop
//         "sbrc %[byte] , 0"        "\n\t" // 1-2  if(b & 0x01)
//          "mov %[n2]   , %[hi]"    "\n\t" // 0-1   n2 = hi
//         "out  %[port] , %[lo]"    "\n\t" // 1    PORT = lo
//         "sbiw %[count], 1"        "\n\t" // 2    i-- (don't act on Z flag yet)
//         // Bit 0:
//         "out  %[port] , %[hi]"    "\n\t" // 1    PORT = hi
//         "mov  %[n1]   , %[lo]"    "\n\t" // 1    n1   = lo
//         "out  %[port] , %[n2]"    "\n\t" // 1    PORT = n2
//         "ld   %[byte] , %a[ptr]+" "\n\t" // 2    b = *ptr++
//         "sbrc %[byte] , 7"        "\n\t" // 1-2  if(b & 0x80)
//          "mov %[n1]   , %[hi]"    "\n\t" // 0-1   n1 = hi
//         "out  %[port] , %[lo]"    "\n\t" // 1    PORT = lo
//         "brne headD"              "\n"   // 2    while(i) (Z flag set above)
//       : [byte]  "+r" (b),
//         [n1]    "+r" (n1),
//         [n2]    "+r" (n2),
//         [count] "+w" (i)
//       : [port]   "I" (_SFR_IO_ADDR(PORTD)),
//         [ptr]    "e" (ptr),
//         [hi]     "r" (hi),
//         [lo]     "r" (lo));

//  #if defined(PORTB) || defined(PORTC) || defined(PORTF)
//     } else // other PORT(s)
//  #endif // defined(PORTB/C/F)
// #endif // defined(PORTD)

//     // PORTB OUTPUT ----------------------------------------------------

// #if defined(PORTB)
//  #if defined(PORTD) || defined(PORTC) || defined(PORTF)
//     if(port == &PORTB) {
//  #endif // defined(PORTD/C/F)

//       // Same as above, just switched to PORTB and stripped of comments.
//       hi = PORTB |  this->pinMask;
//       lo = PORTB & ~this->pinMask;
//       n1 = lo;
//       if(b & 0x80) n1 = hi;

//       asm volatile(
//        "headB:"                   "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "mov  %[n2]   , %[lo]"    "\n\t"
//         "out  %[port] , %[n1]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "sbrc %[byte] , 6"        "\n\t"
//          "mov %[n2]   , %[hi]"    "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "mov  %[n1]   , %[lo]"    "\n\t"
//         "out  %[port] , %[n2]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "sbrc %[byte] , 5"        "\n\t"
//          "mov %[n1]   , %[hi]"    "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "mov  %[n2]   , %[lo]"    "\n\t"
//         "out  %[port] , %[n1]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "sbrc %[byte] , 4"        "\n\t"
//          "mov %[n2]   , %[hi]"    "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "mov  %[n1]   , %[lo]"    "\n\t"
//         "out  %[port] , %[n2]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "sbrc %[byte] , 3"        "\n\t"
//          "mov %[n1]   , %[hi]"    "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "mov  %[n2]   , %[lo]"    "\n\t"
//         "out  %[port] , %[n1]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "sbrc %[byte] , 2"        "\n\t"
//          "mov %[n2]   , %[hi]"    "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "mov  %[n1]   , %[lo]"    "\n\t"
//         "out  %[port] , %[n2]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "sbrc %[byte] , 1"        "\n\t"
//          "mov %[n1]   , %[hi]"    "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "mov  %[n2]   , %[lo]"    "\n\t"
//         "out  %[port] , %[n1]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "sbrc %[byte] , 0"        "\n\t"
//          "mov %[n2]   , %[hi]"    "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "sbiw %[count], 1"        "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "mov  %[n1]   , %[lo]"    "\n\t"
//         "out  %[port] , %[n2]"    "\n\t"
//         "ld   %[byte] , %a[ptr]+" "\n\t"
//         "sbrc %[byte] , 7"        "\n\t"
//          "mov %[n1]   , %[hi]"    "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "brne headB"              "\n"
//       : [byte] "+r" (b), [n1] "+r" (n1), [n2] "+r" (n2), [count] "+w" (i)
//       : [port] "I" (_SFR_IO_ADDR(PORTB)), [ptr] "e" (ptr), [hi] "r" (hi),
//         [lo] "r" (lo));

//  #if defined(PORTD) || defined(PORTC) || defined(PORTF)
//     }
//  #endif
//  #if defined(PORTC) || defined(PORTF)
//     else
//  #endif // defined(PORTC/F)
// #endif // defined(PORTB)

//     // PORTC OUTPUT ----------------------------------------------------

// #if defined(PORTC)
//  #if defined(PORTD) || defined(PORTB) || defined(PORTF)
//     if(port == &PORTC) {
//  #endif // defined(PORTD/B/F)

//       // Same as above, just switched to PORTC and stripped of comments.
//       hi = PORTC |  this->pinMask;
//       lo = PORTC & ~this->pinMask;
//       n1 = lo;
//       if(b & 0x80) n1 = hi;

//       asm volatile(
//        "headC:"                   "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "mov  %[n2]   , %[lo]"    "\n\t"
//         "out  %[port] , %[n1]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "sbrc %[byte] , 6"        "\n\t"
//          "mov %[n2]   , %[hi]"    "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "mov  %[n1]   , %[lo]"    "\n\t"
//         "out  %[port] , %[n2]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "sbrc %[byte] , 5"        "\n\t"
//          "mov %[n1]   , %[hi]"    "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "mov  %[n2]   , %[lo]"    "\n\t"
//         "out  %[port] , %[n1]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "sbrc %[byte] , 4"        "\n\t"
//          "mov %[n2]   , %[hi]"    "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "mov  %[n1]   , %[lo]"    "\n\t"
//         "out  %[port] , %[n2]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "sbrc %[byte] , 3"        "\n\t"
//          "mov %[n1]   , %[hi]"    "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "mov  %[n2]   , %[lo]"    "\n\t"
//         "out  %[port] , %[n1]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "sbrc %[byte] , 2"        "\n\t"
//          "mov %[n2]   , %[hi]"    "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "mov  %[n1]   , %[lo]"    "\n\t"
//         "out  %[port] , %[n2]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "sbrc %[byte] , 1"        "\n\t"
//          "mov %[n1]   , %[hi]"    "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "mov  %[n2]   , %[lo]"    "\n\t"
//         "out  %[port] , %[n1]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "sbrc %[byte] , 0"        "\n\t"
//          "mov %[n2]   , %[hi]"    "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "sbiw %[count], 1"        "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "mov  %[n1]   , %[lo]"    "\n\t"
//         "out  %[port] , %[n2]"    "\n\t"
//         "ld   %[byte] , %a[ptr]+" "\n\t"
//         "sbrc %[byte] , 7"        "\n\t"
//          "mov %[n1]   , %[hi]"    "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "brne headC"              "\n"
//       : [byte] "+r" (b), [n1] "+r" (n1), [n2] "+r" (n2), [count] "+w" (i)
//       : [port] "I" (_SFR_IO_ADDR(PORTC)), [ptr] "e" (ptr), [hi] "r" (hi),
//         [lo] "r" (lo));

//  #if defined(PORTD) || defined(PORTB) || defined(PORTF)
//     }
//  #endif // defined(PORTD/B/F)
//  #if defined(PORTF)
//     else
//  #endif
// #endif // defined(PORTC)

//     // PORTF OUTPUT ----------------------------------------------------

// #if defined(PORTF)
//  #if defined(PORTD) || defined(PORTB) || defined(PORTC)
//     if(port == &PORTF) {
//  #endif // defined(PORTD/B/C)

//       hi = PORTF |  this->pinMask;
//       lo = PORTF & ~this->pinMask;
//       n1 = lo;
//       if(b & 0x80) n1 = hi;

//       asm volatile(
//        "headF:"                   "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "mov  %[n2]   , %[lo]"    "\n\t"
//         "out  %[port] , %[n1]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "sbrc %[byte] , 6"        "\n\t"
//          "mov %[n2]   , %[hi]"    "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "mov  %[n1]   , %[lo]"    "\n\t"
//         "out  %[port] , %[n2]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "sbrc %[byte] , 5"        "\n\t"
//          "mov %[n1]   , %[hi]"    "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "mov  %[n2]   , %[lo]"    "\n\t"
//         "out  %[port] , %[n1]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "sbrc %[byte] , 4"        "\n\t"
//          "mov %[n2]   , %[hi]"    "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "mov  %[n1]   , %[lo]"    "\n\t"
//         "out  %[port] , %[n2]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "sbrc %[byte] , 3"        "\n\t"
//          "mov %[n1]   , %[hi]"    "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "mov  %[n2]   , %[lo]"    "\n\t"
//         "out  %[port] , %[n1]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "sbrc %[byte] , 2"        "\n\t"
//          "mov %[n2]   , %[hi]"    "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "mov  %[n1]   , %[lo]"    "\n\t"
//         "out  %[port] , %[n2]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "sbrc %[byte] , 1"        "\n\t"
//          "mov %[n1]   , %[hi]"    "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "mov  %[n2]   , %[lo]"    "\n\t"
//         "out  %[port] , %[n1]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "sbrc %[byte] , 0"        "\n\t"
//          "mov %[n2]   , %[hi]"    "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "sbiw %[count], 1"        "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "mov  %[n1]   , %[lo]"    "\n\t"
//         "out  %[port] , %[n2]"    "\n\t"
//         "ld   %[byte] , %a[ptr]+" "\n\t"
//         "sbrc %[byte] , 7"        "\n\t"
//          "mov %[n1]   , %[hi]"    "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "brne headF"              "\n"
//       : [byte] "+r" (b), [n1] "+r" (n1), [n2] "+r" (n2), [count] "+w" (i)
//       : [port] "I" (_SFR_IO_ADDR(PORTF)), [ptr] "e" (ptr), [hi] "r" (hi),
//         [lo] "r" (lo));

//  #if defined(PORTD) || defined(PORTB) || defined(PORTC)
//     }
//  #endif // defined(PORTD/B/C)
// #endif // defined(PORTF)

// #ifdef NEO_KHZ400
//   } else { // end 800 KHz, do 400 KHz

//     // Timing is more relaxed; unrolling the inner loop for each bit is
//     // not necessary.  Still using the peculiar RJMPs as 2X NOPs, not out
//     // of need but just to trim the code size down a little.
//     // This 400-KHz-datastream-on-8-MHz-CPU code is not quite identical
//     // to the 800-on-16 code later -- the hi/lo timing between WS2811 and
//     // WS2812 is not simply a 2:1 scale!

//     // 20 inst. clocks per bit: HHHHxxxxxxLLLLLLLLLL
//     // ST instructions:         ^   ^     ^          (T=0,4,10)

//     volatile uint8_t next, bit;

//     hi   = *port |  this->pinMask;
//     lo   = *port & ~this->pinMask;
//     next = lo;
//     bit  = 8;

//     asm volatile(
//      "head20:"                  "\n\t" // Clk  Pseudocode    (T =  0)
//       "st   %a[port], %[hi]"    "\n\t" // 2    PORT = hi     (T =  2)
//       "sbrc %[byte] , 7"        "\n\t" // 1-2  if(b & 128)
//        "mov  %[next], %[hi]"    "\n\t" // 0-1   next = hi    (T =  4)
//       "st   %a[port], %[next]"  "\n\t" // 2    PORT = next   (T =  6)
//       "mov  %[next] , %[lo]"    "\n\t" // 1    next = lo     (T =  7)
//       "dec  %[bit]"             "\n\t" // 1    bit--         (T =  8)
//       "breq nextbyte20"         "\n\t" // 1-2  if(bit == 0)
//       "rol  %[byte]"            "\n\t" // 1    b <<= 1       (T = 10)
//       "st   %a[port], %[lo]"    "\n\t" // 2    PORT = lo     (T = 12)
//       "rjmp .+0"                "\n\t" // 2    nop nop       (T = 14)
//       "rjmp .+0"                "\n\t" // 2    nop nop       (T = 16)
//       "rjmp .+0"                "\n\t" // 2    nop nop       (T = 18)
//       "rjmp head20"             "\n\t" // 2    -> head20 (next bit out)
//      "nextbyte20:"              "\n\t" //                    (T = 10)
//       "st   %a[port], %[lo]"    "\n\t" // 2    PORT = lo     (T = 12)
//       "nop"                     "\n\t" // 1    nop           (T = 13)
//       "ldi  %[bit]  , 8"        "\n\t" // 1    bit = 8       (T = 14)
//       "ld   %[byte] , %a[ptr]+" "\n\t" // 2    b = *ptr++    (T = 16)
//       "sbiw %[count], 1"        "\n\t" // 2    i--           (T = 18)
//       "brne head20"             "\n"   // 2    if(i != 0) -> (next byte)
//       : [port]  "+e" (port),
//         [byte]  "+r" (b),
//         [bit]   "+r" (bit),
//         [next]  "+r" (next),
//         [count] "+w" (i)
//       : [hi]    "r" (hi),
//         [lo]    "r" (lo),
//         [ptr]   "e" (ptr));
//   }
// #endif // NEO_KHZ400

// // 12 MHz(ish) AVR --------------------------------------------------------
// #elif (F_CPU >= 11100000UL) && (F_CPU <= 14300000UL)

// #ifdef NEO_KHZ400 // 800 KHz check needed only if 400 KHz support enabled
//   if(is800KHz) {
// #endif

//     // In the 12 MHz case, an optimized 800 KHz datastream (no dead time
//     // between bytes) requires a PORT-specific loop similar to the 8 MHz
//     // code (but a little more relaxed in this case).

//     // 15 instruction clocks per bit: HHHHxxxxxxLLLLL
//     // OUT instructions:              ^   ^     ^     (T=0,4,10)

//     volatile uint8_t next;

//     // PORTD OUTPUT ----------------------------------------------------

// #if defined(PORTD)
//  #if defined(PORTB) || defined(PORTC) || defined(PORTF)
//     if(port == &PORTD) {
//  #endif // defined(PORTB/C/F)

//       hi   = PORTD |  this->pinMask;
//       lo   = PORTD & ~this->pinMask;
//       next = lo;
//       if(b & 0x80) next = hi;

//       // Don't "optimize" the OUT calls into the bitTime subroutine;
//       // we're exploiting the RCALL and RET as 3- and 4-cycle NOPs!
//       asm volatile(
//        "headD:"                   "\n\t" //        (T =  0)
//         "out   %[port], %[hi]"    "\n\t" //        (T =  1)
//         "rcall bitTimeD"          "\n\t" // Bit 7  (T = 15)
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeD"          "\n\t" // Bit 6
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeD"          "\n\t" // Bit 5
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeD"          "\n\t" // Bit 4
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeD"          "\n\t" // Bit 3
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeD"          "\n\t" // Bit 2
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeD"          "\n\t" // Bit 1
//         // Bit 0:
//         "out  %[port] , %[hi]"    "\n\t" // 1    PORT = hi    (T =  1)
//         "rjmp .+0"                "\n\t" // 2    nop nop      (T =  3)
//         "ld   %[byte] , %a[ptr]+" "\n\t" // 2    b = *ptr++   (T =  5)
//         "out  %[port] , %[next]"  "\n\t" // 1    PORT = next  (T =  6)
//         "mov  %[next] , %[lo]"    "\n\t" // 1    next = lo    (T =  7)
//         "sbrc %[byte] , 7"        "\n\t" // 1-2  if(b & 0x80) (T =  8)
//          "mov %[next] , %[hi]"    "\n\t" // 0-1    next = hi  (T =  9)
//         "nop"                     "\n\t" // 1                 (T = 10)
//         "out  %[port] , %[lo]"    "\n\t" // 1    PORT = lo    (T = 11)
//         "sbiw %[count], 1"        "\n\t" // 2    i--          (T = 13)
//         "brne headD"              "\n\t" // 2    if(i != 0) -> (next byte)
//          "rjmp doneD"             "\n\t"
//         "bitTimeD:"               "\n\t" //      nop nop nop     (T =  4)
//          "out  %[port], %[next]"  "\n\t" // 1    PORT = next     (T =  5)
//          "mov  %[next], %[lo]"    "\n\t" // 1    next = lo       (T =  6)
//          "rol  %[byte]"           "\n\t" // 1    b <<= 1         (T =  7)
//          "sbrc %[byte], 7"        "\n\t" // 1-2  if(b & 0x80)    (T =  8)
//           "mov %[next], %[hi]"    "\n\t" // 0-1   next = hi      (T =  9)
//          "nop"                    "\n\t" // 1                    (T = 10)
//          "out  %[port], %[lo]"    "\n\t" // 1    PORT = lo       (T = 11)
//          "ret"                    "\n\t" // 4    nop nop nop nop (T = 15)
//          "doneD:"                 "\n"
//         : [byte]  "+r" (b),
//           [next]  "+r" (next),
//           [count] "+w" (i)
//         : [port]   "I" (_SFR_IO_ADDR(PORTD)),
//           [ptr]    "e" (ptr),
//           [hi]     "r" (hi),
//           [lo]     "r" (lo));

//  #if defined(PORTB) || defined(PORTC) || defined(PORTF)
//     } else // other PORT(s)
//  #endif // defined(PORTB/C/F)
// #endif // defined(PORTD)

//     // PORTB OUTPUT ----------------------------------------------------

// #if defined(PORTB)
//  #if defined(PORTD) || defined(PORTC) || defined(PORTF)
//     if(port == &PORTB) {
//  #endif // defined(PORTD/C/F)

//       hi   = PORTB |  this->pinMask;
//       lo   = PORTB & ~this->pinMask;
//       next = lo;
//       if(b & 0x80) next = hi;

//       // Same as above, just set for PORTB & stripped of comments
//       asm volatile(
//        "headB:"                   "\n\t"
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeB"          "\n\t"
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeB"          "\n\t"
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeB"          "\n\t"
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeB"          "\n\t"
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeB"          "\n\t"
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeB"          "\n\t"
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeB"          "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "ld   %[byte] , %a[ptr]+" "\n\t"
//         "out  %[port] , %[next]"  "\n\t"
//         "mov  %[next] , %[lo]"    "\n\t"
//         "sbrc %[byte] , 7"        "\n\t"
//          "mov %[next] , %[hi]"    "\n\t"
//         "nop"                     "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "sbiw %[count], 1"        "\n\t"
//         "brne headB"              "\n\t"
//          "rjmp doneB"             "\n\t"
//         "bitTimeB:"               "\n\t"
//          "out  %[port], %[next]"  "\n\t"
//          "mov  %[next], %[lo]"    "\n\t"
//          "rol  %[byte]"           "\n\t"
//          "sbrc %[byte], 7"        "\n\t"
//           "mov %[next], %[hi]"    "\n\t"
//          "nop"                    "\n\t"
//          "out  %[port], %[lo]"    "\n\t"
//          "ret"                    "\n\t"
//          "doneB:"                 "\n"
//         : [byte] "+r" (b), [next] "+r" (next), [count] "+w" (i)
//         : [port] "I" (_SFR_IO_ADDR(PORTB)), [ptr] "e" (ptr), [hi] "r" (hi),
//           [lo] "r" (lo));

//  #if defined(PORTD) || defined(PORTC) || defined(PORTF)
//     }
//  #endif
//  #if defined(PORTC) || defined(PORTF)
//     else
//  #endif // defined(PORTC/F)
// #endif // defined(PORTB)

//     // PORTC OUTPUT ----------------------------------------------------

// #if defined(PORTC)
//  #if defined(PORTD) || defined(PORTB) || defined(PORTF)
//     if(port == &PORTC) {
//  #endif // defined(PORTD/B/F)

//       hi   = PORTC |  this->pinMask;
//       lo   = PORTC & ~this->pinMask;
//       next = lo;
//       if(b & 0x80) next = hi;

//       // Same as above, just set for PORTC & stripped of comments
//       asm volatile(
//        "headC:"                   "\n\t"
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeC"          "\n\t"
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeC"          "\n\t"
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeC"          "\n\t"
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeC"          "\n\t"
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeC"          "\n\t"
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeC"          "\n\t"
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeC"          "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "ld   %[byte] , %a[ptr]+" "\n\t"
//         "out  %[port] , %[next]"  "\n\t"
//         "mov  %[next] , %[lo]"    "\n\t"
//         "sbrc %[byte] , 7"        "\n\t"
//          "mov %[next] , %[hi]"    "\n\t"
//         "nop"                     "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "sbiw %[count], 1"        "\n\t"
//         "brne headC"              "\n\t"
//          "rjmp doneC"             "\n\t"
//         "bitTimeC:"               "\n\t"
//          "out  %[port], %[next]"  "\n\t"
//          "mov  %[next], %[lo]"    "\n\t"
//          "rol  %[byte]"           "\n\t"
//          "sbrc %[byte], 7"        "\n\t"
//           "mov %[next], %[hi]"    "\n\t"
//          "nop"                    "\n\t"
//          "out  %[port], %[lo]"    "\n\t"
//          "ret"                    "\n\t"
//          "doneC:"                 "\n"
//         : [byte] "+r" (b), [next] "+r" (next), [count] "+w" (i)
//         : [port] "I" (_SFR_IO_ADDR(PORTC)), [ptr] "e" (ptr), [hi] "r" (hi),
//           [lo] "r" (lo));

//  #if defined(PORTD) || defined(PORTB) || defined(PORTF)
//     }
//  #endif // defined(PORTD/B/F)
//  #if defined(PORTF)
//     else
//  #endif
// #endif // defined(PORTC)

//     // PORTF OUTPUT ----------------------------------------------------

// #if defined(PORTF)
//  #if defined(PORTD) || defined(PORTB) || defined(PORTC)
//     if(port == &PORTF) {
//  #endif // defined(PORTD/B/C)

//       hi   = PORTF |  this->pinMask;
//       lo   = PORTF & ~this->pinMask;
//       next = lo;
//       if(b & 0x80) next = hi;

//       // Same as above, just set for PORTF & stripped of comments
//       asm volatile(
//        "headF:"                   "\n\t"
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeC"          "\n\t"
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeC"          "\n\t"
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeC"          "\n\t"
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeC"          "\n\t"
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeC"          "\n\t"
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeC"          "\n\t"
//         "out   %[port], %[hi]"    "\n\t"
//         "rcall bitTimeC"          "\n\t"
//         "out  %[port] , %[hi]"    "\n\t"
//         "rjmp .+0"                "\n\t"
//         "ld   %[byte] , %a[ptr]+" "\n\t"
//         "out  %[port] , %[next]"  "\n\t"
//         "mov  %[next] , %[lo]"    "\n\t"
//         "sbrc %[byte] , 7"        "\n\t"
//          "mov %[next] , %[hi]"    "\n\t"
//         "nop"                     "\n\t"
//         "out  %[port] , %[lo]"    "\n\t"
//         "sbiw %[count], 1"        "\n\t"
//         "brne headF"              "\n\t"
//          "rjmp doneC"             "\n\t"
//         "bitTimeC:"               "\n\t"
//          "out  %[port], %[next]"  "\n\t"
//          "mov  %[next], %[lo]"    "\n\t"
//          "rol  %[byte]"           "\n\t"
//          "sbrc %[byte], 7"        "\n\t"
//           "mov %[next], %[hi]"    "\n\t"
//          "nop"                    "\n\t"
//          "out  %[port], %[lo]"    "\n\t"
//          "ret"                    "\n\t"
//          "doneC:"                 "\n"
//         : [byte] "+r" (b), [next] "+r" (next), [count] "+w" (i)
//         : [port] "I" (_SFR_IO_ADDR(PORTF)), [ptr] "e" (ptr), [hi] "r" (hi),
//           [lo] "r" (lo));

//  #if defined(PORTD) || defined(PORTB) || defined(PORTC)
//     }
//  #endif // defined(PORTD/B/C)
// #endif // defined(PORTF)

// #ifdef NEO_KHZ400
//   } else { // 400 KHz

//     // 30 instruction clocks per bit: HHHHHHxxxxxxxxxLLLLLLLLLLLLLLL
//     // ST instructions:               ^     ^        ^    (T=0,6,15)

//     volatile uint8_t next, bit;

//     hi   = *port |  this->pinMask;
//     lo   = *port & ~this->pinMask;
//     next = lo;
//     bit  = 8;

//     asm volatile(
//      "head30:"                  "\n\t" // Clk  Pseudocode    (T =  0)
//       "st   %a[port], %[hi]"    "\n\t" // 2    PORT = hi     (T =  2)
//       "sbrc %[byte] , 7"        "\n\t" // 1-2  if(b & 128)
//        "mov  %[next], %[hi]"    "\n\t" // 0-1   next = hi    (T =  4)
//       "rjmp .+0"                "\n\t" // 2    nop nop       (T =  6)
//       "st   %a[port], %[next]"  "\n\t" // 2    PORT = next   (T =  8)
//       "rjmp .+0"                "\n\t" // 2    nop nop       (T = 10)
//       "rjmp .+0"                "\n\t" // 2    nop nop       (T = 12)
//       "rjmp .+0"                "\n\t" // 2    nop nop       (T = 14)
//       "nop"                     "\n\t" // 1    nop           (T = 15)
//       "st   %a[port], %[lo]"    "\n\t" // 2    PORT = lo     (T = 17)
//       "rjmp .+0"                "\n\t" // 2    nop nop       (T = 19)
//       "dec  %[bit]"             "\n\t" // 1    bit--         (T = 20)
//       "breq nextbyte30"         "\n\t" // 1-2  if(bit == 0)
//       "rol  %[byte]"            "\n\t" // 1    b <<= 1       (T = 22)
//       "rjmp .+0"                "\n\t" // 2    nop nop       (T = 24)
//       "rjmp .+0"                "\n\t" // 2    nop nop       (T = 26)
//       "rjmp .+0"                "\n\t" // 2    nop nop       (T = 28)
//       "rjmp head30"             "\n\t" // 2    -> head30 (next bit out)
//      "nextbyte30:"              "\n\t" //                    (T = 22)
//       "nop"                     "\n\t" // 1    nop           (T = 23)
//       "ldi  %[bit]  , 8"        "\n\t" // 1    bit = 8       (T = 24)
//       "ld   %[byte] , %a[ptr]+" "\n\t" // 2    b = *ptr++    (T = 26)
//       "sbiw %[count], 1"        "\n\t" // 2    i--           (T = 28)
//       "brne head30"             "\n"   // 1-2  if(i != 0) -> (next byte)
//       : [port]  "+e" (port),
//         [byte]  "+r" (b),
//         [bit]   "+r" (bit),
//         [next]  "+r" (next),
//         [count] "+w" (i)
//       : [hi]     "r" (hi),
//         [lo]     "r" (lo),
//         [ptr]    "e" (ptr));
//   }
// #endif // NEO_KHZ400

// // 16 MHz(ish) AVR --------------------------------------------------------
// #elif (F_CPU >= 15400000UL) && (F_CPU <= 19000000L)

// #ifdef NEO_KHZ400 // 800 KHz check needed only if 400 KHz support enabled
//   if(is800KHz) {
// #endif

//     // WS2811 and WS2812 have different hi/lo duty cycles; this is
//     // similar but NOT an exact copy of the prior 400-on-8 code.

//     // 20 inst. clocks per bit: HHHHHxxxxxxxxLLLLLLL
//     // ST instructions:         ^   ^        ^       (T=0,5,13)

//     volatile uint8_t next, bit;

//     hi   = *port |  this->pinMask;
//     lo   = *port & ~this->pinMask;
//     next = lo;
//     bit  = 8;

//     asm volatile(
//      "head20:"                   "\n\t" // Clk  Pseudocode    (T =  0)
//       "st   %a[port],  %[hi]"    "\n\t" // 2    PORT = hi     (T =  2)
//       "sbrc %[byte],  7"         "\n\t" // 1-2  if(b & 128)
//        "mov  %[next], %[hi]"     "\n\t" // 0-1   next = hi    (T =  4)
//       "dec  %[bit]"              "\n\t" // 1    bit--         (T =  5)
//       "st   %a[port],  %[next]"  "\n\t" // 2    PORT = next   (T =  7)
//       "mov  %[next] ,  %[lo]"    "\n\t" // 1    next = lo     (T =  8)
//       "breq nextbyte20"          "\n\t" // 1-2  if(bit == 0) (from dec above)
//       "rol  %[byte]"             "\n\t" // 1    b <<= 1       (T = 10)
//       "rjmp .+0"                 "\n\t" // 2    nop nop       (T = 12)
//       "nop"                      "\n\t" // 1    nop           (T = 13)
//       "st   %a[port],  %[lo]"    "\n\t" // 2    PORT = lo     (T = 15)
//       "nop"                      "\n\t" // 1    nop           (T = 16)
//       "rjmp .+0"                 "\n\t" // 2    nop nop       (T = 18)
//       "rjmp head20"              "\n\t" // 2    -> head20 (next bit out)
//      "nextbyte20:"               "\n\t" //                    (T = 10)
//       "ldi  %[bit]  ,  8"        "\n\t" // 1    bit = 8       (T = 11)
//       "ld   %[byte] ,  %a[ptr]+" "\n\t" // 2    b = *ptr++    (T = 13)
//       "st   %a[port], %[lo]"     "\n\t" // 2    PORT = lo     (T = 15)
//       "nop"                      "\n\t" // 1    nop           (T = 16)
//       "sbiw %[count], 1"         "\n\t" // 2    i--           (T = 18)
//        "brne head20"             "\n"   // 2    if(i != 0) -> (next byte)
//       : [port]  "+e" (port),
//         [byte]  "+r" (b),
//         [bit]   "+r" (bit),
//         [next]  "+r" (next),
//         [count] "+w" (i)
//       : [ptr]    "e" (ptr),
//         [hi]     "r" (hi),
//         [lo]     "r" (lo));

// #ifdef NEO_KHZ400
//   } else { // 400 KHz

//     // The 400 KHz clock on 16 MHz MCU is the most 'relaxed' version.

//     // 40 inst. clocks per bit: HHHHHHHHxxxxxxxxxxxxLLLLLLLLLLLLLLLLLLLL
//     // ST instructions:         ^       ^           ^         (T=0,8,20)

//     volatile uint8_t next, bit;

//     hi   = *port |  this->pinMask;
//     lo   = *port & ~this->pinMask;
//     next = lo;
//     bit  = 8;

//     asm volatile(
//      "head40:"                  "\n\t" // Clk  Pseudocode    (T =  0)
//       "st   %a[port], %[hi]"    "\n\t" // 2    PORT = hi     (T =  2)
//       "sbrc %[byte] , 7"        "\n\t" // 1-2  if(b & 128)
//        "mov  %[next] , %[hi]"   "\n\t" // 0-1   next = hi    (T =  4)
//       "rjmp .+0"                "\n\t" // 2    nop nop       (T =  6)
//       "rjmp .+0"                "\n\t" // 2    nop nop       (T =  8)
//       "st   %a[port], %[next]"  "\n\t" // 2    PORT = next   (T = 10)
//       "rjmp .+0"                "\n\t" // 2    nop nop       (T = 12)
//       "rjmp .+0"                "\n\t" // 2    nop nop       (T = 14)
//       "rjmp .+0"                "\n\t" // 2    nop nop       (T = 16)
//       "rjmp .+0"                "\n\t" // 2    nop nop       (T = 18)
//       "rjmp .+0"                "\n\t" // 2    nop nop       (T = 20)
//       "st   %a[port], %[lo]"    "\n\t" // 2    PORT = lo     (T = 22)
//       "nop"                     "\n\t" // 1    nop           (T = 23)
//       "mov  %[next] , %[lo]"    "\n\t" // 1    next = lo     (T = 24)
//       "dec  %[bit]"             "\n\t" // 1    bit--         (T = 25)
//       "breq nextbyte40"         "\n\t" // 1-2  if(bit == 0)
//       "rol  %[byte]"            "\n\t" // 1    b <<= 1       (T = 27)
//       "nop"                     "\n\t" // 1    nop           (T = 28)
//       "rjmp .+0"                "\n\t" // 2    nop nop       (T = 30)
//       "rjmp .+0"                "\n\t" // 2    nop nop       (T = 32)
//       "rjmp .+0"                "\n\t" // 2    nop nop       (T = 34)
//       "rjmp .+0"                "\n\t" // 2    nop nop       (T = 36)
//       "rjmp .+0"                "\n\t" // 2    nop nop       (T = 38)
//       "rjmp head40"             "\n\t" // 2    -> head40 (next bit out)
//      "nextbyte40:"              "\n\t" //                    (T = 27)
//       "ldi  %[bit]  , 8"        "\n\t" // 1    bit = 8       (T = 28)
//       "ld   %[byte] , %a[ptr]+" "\n\t" // 2    b = *ptr++    (T = 30)
//       "rjmp .+0"                "\n\t" // 2    nop nop       (T = 32)
//       "st   %a[port], %[lo]"    "\n\t" // 2    PORT = lo     (T = 34)
//       "rjmp .+0"                "\n\t" // 2    nop nop       (T = 36)
//       "sbiw %[count], 1"        "\n\t" // 2    i--           (T = 38)
//       "brne head40"             "\n"   // 1-2  if(i != 0) -> (next byte)
//       : [port]  "+e" (port),
//         [byte]  "+r" (b),
//         [bit]   "+r" (bit),
//         [next]  "+r" (next),
//         [count] "+w" (i)
//       : [ptr]    "e" (ptr),
//         [hi]     "r" (hi),
//         [lo]     "r" (lo));
//   }
// #endif // NEO_KHZ400

// #else
//  #error "CPU SPEED NOT SUPPORTED"
// #endif // end F_CPU ifdefs on __AVR__

// // END AVR ----------------------------------------------------------------

// #elif defined(__arm__)

// // ARM MCUs -- Teensy 3.0, 3.1, LC, Arduino Due ---------------------------

// #if defined(__MK20DX128__) || defined(__MK20DX256__) // Teensy 3.0 & 3.1
// #define CYCLES_800_T0H  (F_CPU / 4000000)
// #define CYCLES_800_T1H  (F_CPU / 1250000)
// #define CYCLES_800      (F_CPU /  800000)
// #define CYCLES_400_T0H  (F_CPU / 2000000)
// #define CYCLES_400_T1H  (F_CPU /  833333)
// #define CYCLES_400      (F_CPU /  400000)

//   uint8_t          *p   = this->pixels,
//                    *end = p + this->numBytes, pix, mask;
//   volatile uint8_t *set = portSetRegister(this->pin),
//                    *clr = portClearRegister(this->pin);
//   uint32_t          cyc;

//   ARM_DEMCR    |= ARM_DEMCR_TRCENA;
//   ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;

// #ifdef NEO_KHZ400 // 800 KHz check needed only if 400 KHz support enabled
//   if(is800KHz) {
// #endif
//     cyc = ARM_DWT_CYCCNT + CYCLES_800;
//     while(p < end) {
//       pix = *p++;
//       for(mask = 0x80; mask; mask >>= 1) {
//         while(ARM_DWT_CYCCNT - cyc < CYCLES_800);
//         cyc  = ARM_DWT_CYCCNT;
//         *set = 1;
//         if(pix & mask) {
//           while(ARM_DWT_CYCCNT - cyc < CYCLES_800_T1H);
//         } else {
//           while(ARM_DWT_CYCCNT - cyc < CYCLES_800_T0H);
//         }
//         *clr = 1;
//       }
//     }
//     while(ARM_DWT_CYCCNT - cyc < CYCLES_800);
// #ifdef NEO_KHZ400
//   } else { // 400 kHz bitstream
//     cyc = ARM_DWT_CYCCNT + CYCLES_400;
//     while(p < end) {
//       pix = *p++;
//       for(mask = 0x80; mask; mask >>= 1) {
//         while(ARM_DWT_CYCCNT - cyc < CYCLES_400);
//         cyc  = ARM_DWT_CYCCNT;
//         *set = 1;
//         if(pix & mask) {
//           while(ARM_DWT_CYCCNT - cyc < CYCLES_400_T1H);
//         } else {
//           while(ARM_DWT_CYCCNT - cyc < CYCLES_400_T0H);
//         }
//         *clr = 1;
//       }
//     }
//     while(ARM_DWT_CYCCNT - cyc < CYCLES_400);
//   }
// #endif // NEO_KHZ400

// #elif defined(__MKL26Z64__) // Teensy-LC

// #if F_CPU == 48000000
//   uint8_t          *p   = this->pixels,
// 		   pix, count, dly,
//                    bitmask = digitalPinToBitMask(this->pin);
//   volatile uint8_t *reg = portSetRegister(this->pin);
//   uint32_t         num = this->numBytes;
//   asm volatile(
// 	"L%=_begin:"				"\n\t"
// 	"ldrb	%[pix], [%[p], #0]"		"\n\t"
// 	"lsl	%[pix], #24"			"\n\t"
// 	"movs	%[count], #7"			"\n\t"
// 	"L%=_loop:"				"\n\t"
// 	"lsl	%[pix], #1"			"\n\t"
// 	"bcs	L%=_loop_one"			"\n\t"
// 	"L%=_loop_zero:"
// 	"strb	%[bitmask], [%[reg], #0]"	"\n\t"
// 	"movs	%[dly], #4"			"\n\t"
// 	"L%=_loop_delay_T0H:"			"\n\t"
// 	"sub	%[dly], #1"			"\n\t"
// 	"bne	L%=_loop_delay_T0H"		"\n\t"
// 	"strb	%[bitmask], [%[reg], #4]"	"\n\t"
// 	"movs	%[dly], #13"			"\n\t"
// 	"L%=_loop_delay_T0L:"			"\n\t"
// 	"sub	%[dly], #1"			"\n\t"
// 	"bne	L%=_loop_delay_T0L"		"\n\t"
// 	"b	L%=_next"			"\n\t"
// 	"L%=_loop_one:"
// 	"strb	%[bitmask], [%[reg], #0]"	"\n\t"
// 	"movs	%[dly], #13"			"\n\t"
// 	"L%=_loop_delay_T1H:"			"\n\t"
// 	"sub	%[dly], #1"			"\n\t"
// 	"bne	L%=_loop_delay_T1H"		"\n\t"
// 	"strb	%[bitmask], [%[reg], #4]"	"\n\t"
// 	"movs	%[dly], #4"			"\n\t"
// 	"L%=_loop_delay_T1L:"			"\n\t"
// 	"sub	%[dly], #1"			"\n\t"
// 	"bne	L%=_loop_delay_T1L"		"\n\t"
// 	"nop"					"\n\t"
// 	"L%=_next:"				"\n\t"
// 	"sub	%[count], #1"			"\n\t"
// 	"bne	L%=_loop"			"\n\t"
// 	"lsl	%[pix], #1"			"\n\t"
// 	"bcs	L%=_last_one"			"\n\t"
// 	"L%=_last_zero:"
// 	"strb	%[bitmask], [%[reg], #0]"	"\n\t"
// 	"movs	%[dly], #4"			"\n\t"
// 	"L%=_last_delay_T0H:"			"\n\t"
// 	"sub	%[dly], #1"			"\n\t"
// 	"bne	L%=_last_delay_T0H"		"\n\t"
// 	"strb	%[bitmask], [%[reg], #4]"	"\n\t"
// 	"movs	%[dly], #10"			"\n\t"
// 	"L%=_last_delay_T0L:"			"\n\t"
// 	"sub	%[dly], #1"			"\n\t"
// 	"bne	L%=_last_delay_T0L"		"\n\t"
// 	"b	L%=_repeat"			"\n\t"
// 	"L%=_last_one:"
// 	"strb	%[bitmask], [%[reg], #0]"	"\n\t"
// 	"movs	%[dly], #13"			"\n\t"
// 	"L%=_last_delay_T1H:"			"\n\t"
// 	"sub	%[dly], #1"			"\n\t"
// 	"bne	L%=_last_delay_T1H"		"\n\t"
// 	"strb	%[bitmask], [%[reg], #4]"	"\n\t"
// 	"movs	%[dly], #1"			"\n\t"
// 	"L%=_last_delay_T1L:"			"\n\t"
// 	"sub	%[dly], #1"			"\n\t"
// 	"bne	L%=_last_delay_T1L"		"\n\t"
// 	"nop"					"\n\t"
// 	"L%=_repeat:"				"\n\t"
// 	"add	%[p], #1"			"\n\t"
// 	"sub	%[num], #1"			"\n\t"
// 	"bne	L%=_begin"			"\n\t"
// 	"L%=_done:"				"\n\t"
// 	: [p] "+r" (p),
// 	  [pix] "=&r" (pix),
// 	  [count] "=&r" (count),
// 	  [dly] "=&r" (dly),
// 	  [num] "+r" (num)
// 	: [bitmask] "r" (bitmask),
// 	  [reg] "r" (reg)
//   );
// #else
// #error "Sorry, only 48 MHz is supported, please set Tools > CPU Speed to 48 MHz"
// #endif // F_CPU == 48000000

// #elif defined(__SAMD21G18A__)  || defined(__SAMD21E18A__) || defined(__SAMD21J18A__) // Arduino Zero, Gemma/Trinket M0, SODAQ Autonomo and others
//   // Tried this with a timer/counter, couldn't quite get adequate
//   // resolution.  So yay, you get a load of goofball NOPs...

//   uint8_t  *ptr, *end, p, bitMask, portNum;
//   uint32_t  this->pinMask;

//   portNum =  g_APinDescription[this->pin].ulPort;
//   this->pinMask =  1ul << g_APinDescription[this->pin].ulPin;
//   ptr     =  this->pixels;
//   end     =  ptr + this->numBytes;
//   p       = *ptr++;
//   bitMask =  0x80;

//   volatile uint32_t *set = &(PORT->Group[portNum].OUTSET.reg),
//                     *clr = &(PORT->Group[portNum].OUTCLR.reg);

// #ifdef NEO_KHZ400 // 800 KHz check needed only if 400 KHz support enabled
//   if(is800KHz) {
// #endif
//     for(;;) {
//       *set = this->pinMask;
//       asm("nop; nop; nop; nop; nop; nop; nop; nop;");
//       if(p & bitMask) {
//         asm("nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop;");
//         *clr = this->pinMask;
//       } else {
//         *clr = this->pinMask;
//         asm("nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop;");
//       }
//       if(bitMask >>= 1) {
//         asm("nop; nop; nop; nop; nop; nop; nop; nop; nop;");
//       } else {
//         if(ptr >= end) break;
//         p       = *ptr++;
//         bitMask = 0x80;
//       }
//     }
// #ifdef NEO_KHZ400
//   } else { // 400 KHz bitstream
//     for(;;) {
//       *set = this->pinMask;
//       asm("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
//       if(p & bitMask) {
//         asm("nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop;");
//         *clr = this->pinMask;
//       } else {
//         *clr = this->pinMask;
//         asm("nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop;");
//       }
//       asm("nop; nop; nop; nop; nop; nop; nop; nop;"
//           "nop; nop; nop; nop; nop; nop; nop; nop;"
//           "nop; nop; nop; nop; nop; nop; nop; nop;"
//           "nop; nop; nop; nop; nop; nop; nop; nop;");
//       if(bitMask >>= 1) {
//         asm("nop; nop; nop; nop; nop; nop; nop;");
//       } else {
//         if(ptr >= end) break;
//         p       = *ptr++;
//         bitMask = 0x80;
//       }
//     }
//   }
// #endif

// #elif defined (ARDUINO_STM32_FEATHER) // FEATHER WICED (120MHz)

//   // Tried this with a timer/counter, couldn't quite get adequate
//   // resolution.  So yay, you get a load of goofball NOPs...

//   uint8_t  *ptr, *end, p, bitMask;
//   uint32_t  this->pinMask;

//   this->pinMask =  BIT(PIN_MAP[this->pin].gpio_bit);
//   ptr     =  this->pixels;
//   end     =  ptr + this->numBytes;
//   p       = *ptr++;
//   bitMask =  0x80;

//   volatile uint16_t *set = &(PIN_MAP[this->pin].gpio_device->regs->BSRRL);
//   volatile uint16_t *clr = &(PIN_MAP[this->pin].gpio_device->regs->BSRRH);

// #ifdef NEO_KHZ400 // 800 KHz check needed only if 400 KHz support enabled
//   if(is800KHz) {
// #endif
//     for(;;) {
//       if(p & bitMask) { // ONE
//         // High 800ns
//         *set = this->pinMask;
//         asm("nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop;");
//         // Low 450ns
//         *clr = this->pinMask;
//         asm("nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop;");
//       } else { // ZERO
//         // High 400ns
//         *set = this->pinMask;
//         asm("nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop;");
//         // Low 850ns
//         *clr = this->pinMask;
//         asm("nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop; nop; nop; nop; nop;"
//             "nop; nop; nop; nop;");
//       }
//       if(bitMask >>= 1) {
//         // Move on to the next pixel
//         asm("nop;");
//       } else {
//         if(ptr >= end) break;
//         p       = *ptr++;
//         bitMask = 0x80;
//       }
//     }
// #ifdef NEO_KHZ400
//   } else { // 400 KHz bitstream
//     // ToDo!
//   }
// #endif

// #else // Other ARM architecture -- Presumed Arduino Due

//   #define SCALE      VARIANT_MCK / 2UL / 1000000UL
//   #define INST       (2UL * F_CPU / VARIANT_MCK)
//   #define TIME_800_0 ((int)(0.40 * SCALE + 0.5) - (5 * INST))
//   #define TIME_800_1 ((int)(0.80 * SCALE + 0.5) - (5 * INST))
//   #define PERIOD_800 ((int)(1.25 * SCALE + 0.5) - (5 * INST))
//   #define TIME_400_0 ((int)(0.50 * SCALE + 0.5) - (5 * INST))
//   #define TIME_400_1 ((int)(1.20 * SCALE + 0.5) - (5 * INST))
//   #define PERIOD_400 ((int)(2.50 * SCALE + 0.5) - (5 * INST))

//   int             this->pinMask, time0, time1, period, t;
//   Pio            *port;
//   volatile WoReg *portSet, *portClear, *timeValue, *timeReset;
//   uint8_t        *p, *end, pix, mask;

//   pmc_set_writeprotect(false);
//   pmc_enable_periph_clk((uint32_t)TC3_IRQn);
//   TC_Configure(TC1, 0,
//     TC_CMR_WAVE | TC_CMR_WAVSEL_UP | TC_CMR_TCCLKS_TIMER_CLOCK1);
//   TC_Start(TC1, 0);

//   this->pinMask   = g_APinDescription[this->pin].ulPin; // Don't 'optimize' these into
//   port      = g_APinDescription[this->pin].pPort; // declarations above.  Want to
//   portSet   = &(port->PIO_SODR);            // burn a few cycles after
//   portClear = &(port->PIO_CODR);            // starting timer to minimize
//   timeValue = &(TC1->TC_CHANNEL[0].TC_CV);  // the initial 'while'.
//   timeReset = &(TC1->TC_CHANNEL[0].TC_CCR);
//   p         =  this->pixels;
//   end       =  p + this->numBytes;
//   pix       = *p++;
//   mask      = 0x80;

// #ifdef NEO_KHZ400 // 800 KHz check needed only if 400 KHz support enabled
//   if(is800KHz) {
// #endif
//     time0  = TIME_800_0;
//     time1  = TIME_800_1;
//     period = PERIOD_800;
// #ifdef NEO_KHZ400
//   } else { // 400 KHz bitstream
//     time0  = TIME_400_0;
//     time1  = TIME_400_1;
//     period = PERIOD_400;
//   }
// #endif

//   for(t = time0;; t = time0) {
//     if(pix & mask) t = time1;
//     while(*timeValue < period);
//     *portSet   = this->pinMask;
//     *timeReset = TC_CCR_CLKEN | TC_CCR_SWTRG;
//     while(*timeValue < t);
//     *portClear = this->pinMask;
//     if(!(mask >>= 1)) {   // This 'inside-out' loop logic utilizes
//       if(p >= end) break; // idle time to minimize inter-byte delays.
//       pix = *p++;
//       mask = 0x80;
//     }
//   }
//   while(*timeValue < period); // Wait for last bit
//   TC_Stop(TC1, 0);

// #endif // end Due

// // END ARM ----------------------------------------------------------------

// #elif defined(MONGOOSE_OS_ADAFRUIT_NEOPIXEL_ESP8266)

// // ESP8266 ----------------------------------------------------------------

//   // ESP8266 show() is external to enforce ICACHE_RAM_ATTR execution
//   espShow(this->pin, this->pixels, this->numBytes, is800KHz);

// #elif defined(__ARDUINO_ARC__)

// // Arduino 101  -----------------------------------------------------------

// #define NOPx7 { __builtin_arc_nop(); \
//   __builtin_arc_nop(); __builtin_arc_nop(); \
//   __builtin_arc_nop(); __builtin_arc_nop(); \
//   __builtin_arc_nop(); __builtin_arc_nop(); }

//   PinDescription *pindesc = &g_APinDescription[this->pin];
//   register uint32_t loop = 8 * this->numBytes; // one loop to handle all bytes and all bits
//   register uint8_t *p = this->pixels;
//   register uint32_t currByte = (uint32_t) (*p);
//   register uint32_t currBit = 0x80 & currByte;
//   register uint32_t bitCounter = 0;
//   register uint32_t first = 1;

//   // The loop is unusual. Very first iteration puts all the way LOW to the wire -
//   // constant LOW does not affect NEOPIXEL, so there is no visible effect displayed.
//   // During that very first iteration CPU caches instructions in the loop.
//   // Because of the caching process, "CPU slows down". NEOPIXEL pulse is very time sensitive
//   // that's why we let the CPU cache first and we start regular pulse from 2nd iteration
//   if (pindesc->ulGPIOType == SS_GPIO) {
//     register uint32_t reg = pindesc->ulGPIOBase + SS_GPIO_SWPORTA_DR;
//     uint32_t reg_val = __builtin_arc_lr((volatile uint32_t)reg);
//     register uint32_t reg_bit_high = reg_val | (1 << pindesc->ulGPIOId);
//     register uint32_t reg_bit_low  = reg_val & ~(1 << pindesc->ulGPIOId);

//     loop += 1; // include first, special iteration
//     while(loop--) {
//       if(!first) {
//         currByte <<= 1;
//         bitCounter++;
//       }

//       // 1 is >550ns high and >450ns low; 0 is 200..500ns high and >450ns low
//       __builtin_arc_sr(first ? reg_bit_low : reg_bit_high, (volatile uint32_t)reg);
//       if(currBit) { // ~400ns HIGH (740ns overall)
//         NOPx7
//         NOPx7
//       }
//       // ~340ns HIGH
//       NOPx7
//      __builtin_arc_nop();

//       // 820ns LOW; per spec, max allowed low here is 5000ns */
//       __builtin_arc_sr(reg_bit_low, (volatile uint32_t)reg);
//       NOPx7
//       NOPx7

//       if(bitCounter >= 8) {
//         bitCounter = 0;
//         currByte = (uint32_t) (*++p);
//       }

//       currBit = 0x80 & currByte;
//       first = 0;
//     }
//   } else if(pindesc->ulGPIOType == SOC_GPIO) {
//     register uint32_t reg = pindesc->ulGPIOBase + SOC_GPIO_SWPORTA_DR;
//     uint32_t reg_val = MMIO_REG_VAL(reg);
//     register uint32_t reg_bit_high = reg_val | (1 << pindesc->ulGPIOId);
//     register uint32_t reg_bit_low  = reg_val & ~(1 << pindesc->ulGPIOId);

//     loop += 1; // include first, special iteration
//     while(loop--) {
//       if(!first) {
//         currByte <<= 1;
//         bitCounter++;
//       }
//       MMIO_REG_VAL(reg) = first ? reg_bit_low : reg_bit_high;
//       if(currBit) { // ~430ns HIGH (740ns overall)
//         NOPx7
//         NOPx7
//         __builtin_arc_nop();
//       }
//       // ~310ns HIGH
//       NOPx7

//       // 850ns LOW; per spec, max allowed low here is 5000ns */
//       MMIO_REG_VAL(reg) = reg_bit_low;
//       NOPx7
//       NOPx7

//       if(bitCounter >= 8) {
//         bitCounter = 0;
//         currByte = (uint32_t) (*++p);
//       }

//       currBit = 0x80 & currByte;
//       first = 0;
//     }
//   }

// #endif

// // END ARCHITECTURE SELECT ------------------------------------------------

//   interrupts();
//   endTime = micros(); // Save EOD time for latch on next call
// }
