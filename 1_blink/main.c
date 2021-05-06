#include <stdint.h>
#include "rp2040.h"

void cpu0Phase3Boot (void);

void wait()
{
	volatile uint32_t i;
	for(i=0;i<3000000;i++);
}
#define LED_BUILTIN 25

int main()
{
  sio_hw->gpio_oe_set = 1ul << LED_BUILTIN; // enable output
  sio_hw->gpio_clr = 1ul << LED_BUILTIN;    // clear
  iobank0_hw->io[LED_BUILTIN].ctrl = 5; //5 is alternate function for SIO
  while(1)
  {
    sio_hw->gpio_togl = 1ul << LED_BUILTIN;    // toggle
	wait();
  }
}

