#include <stdint.h>
#include "rp2040.h"

//#define LED_BUILTIN 25
const int LED_BUILTIN=25;

//called each 100ms
void isr_systick()
{
    sio_hw->gpio_togl = 1ul << LED_BUILTIN;    // toggle
}

/* max value 0xFFFFFF => ~16 000 000 */
int configure_systick(uint32_t ticks)
{
  /* Reload value impossible */
  if ((ticks - 1) > M0PLUS_SYST_RVR_BITS) return -1;
  /* Reload value ok, set reload register */
  systick_hw->rvr = ticks - 1;
  /* Load the SysTick Counter Value */
  systick_hw->cvr = 0;
  /* Enable SysTick IRQ and SysTick Timer */
  systick_hw->csr = 1 << M0PLUS_SYST_CSR_CLKSOURCE_LSB |
	                1 << M0PLUS_SYST_CSR_TICKINT_LSB   |
	                1 << M0PLUS_SYST_CSR_ENABLE_LSB;
  return 0;
}

int main()
{
  //gpio 25 as output
  sio_hw->gpio_oe_set = 1ul << LED_BUILTIN; // enable output
  sio_hw->gpio_clr = 1ul << LED_BUILTIN;    // clear
  iobank0_hw->io[LED_BUILTIN].ctrl = 5; //5 is alternate function for SIO
  //systick
  if(configure_systick(12500000) != 0) //0.1s
  {
    sio_hw->gpio_set = 1ul << LED_BUILTIN;    //light on if pb.
  }

  while(1)
  {
	  //nothing in background.
  }
}

