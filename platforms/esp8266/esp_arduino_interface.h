#ifndef _ESP_ARDUINO_INTERFACE_H_
#define _ESP_ARDUINO_INTERFACE_H_

#define xt_rsil(level) (__extension__({uint32_t state; __asm__ __volatile__("rsil %0," __STRINGIFY(level) : "=a" (state)); state; }))

#define interrupts() xt_rsil(0)
#define noInterrupts() xt_rsil(15)

#define _BV(b) (1UL << (b))


#define F_CPU (system_get_cpu_freq()*1000000L)


#endif
