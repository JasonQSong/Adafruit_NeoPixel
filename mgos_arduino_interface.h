#ifndef _MGOS_ARDUINO_INTERFACE_H_
#define _MGOS_ARDUINO_INTERFACE_H_

#define pinMode mgos_gpio_set_mode
#define INPUT MGOS_GPIO_MODE_INPUT
#define OUTPUT MGOS_GPIO_MODE_OUTPUT

#define LOW 0
#define digitalWrite mgos_gpio_write

#endif
