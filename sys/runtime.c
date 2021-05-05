/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

//#include <stdio.h>
//#include <stdarg.h>
#include "rp2040.h"

//static const uint32_t XOSC_MHZ = 12 ; // Crystal on Raspberry Pi Pico is 12 MHz
static const uint32_t MHZ = 1000 * 1000 ;
//#include "pico.h"
//
//#include "hardware/regs/m0plus.h"
//#include "hardware/regs/resets.h"
//#include "hardware/structs/mpu.h"
//#include "hardware/structs/scb.h"
//#include "hardware/structs/padsbank0.h"
//
//#include "hardware/clocks.h"
//#include "hardware/irq.h"
//#include "hardware/resets.h"
//
//#include "pico/mutex.h"
//#include "pico/time.h"
//#include "pico/printf.h"
//
//#if PICO_ENTER_USB_BOOT_ON_EXIT
//#include "pico/bootrom.h"
//#endif
//
//#ifndef PICO_NO_RAM_VECTOR_TABLE
//#define PICO_NO_RAM_VECTOR_TABLE 0
//#endif
//
//extern char __StackLimit; /* Set by linker.  */
//
//uint32_t __attribute__((section(".ram_vector_table"))) ram_vector_table[48];

/* from pico-sdk/src/rp2_common/hardware_xosc/xosc.c */
void xosc_init(void) {
    // Assumes 1-15 MHz input
    //assert(XOSC_MHZ <= 15);
    xosc_hw->ctrl = XOSC_CTRL_FREQ_RANGE_VALUE_1_15MHZ;

    // Set xosc startup delay
    uint32_t startup_delay = (((12 * MHZ) / 1000) + 128) / 256;
    xosc_hw->startup = startup_delay;

    // Set the enable bit now that we have set freq range and startup delay
    xosc_hw->ctrl |= XOSC_CTRL_ENABLE_VALUE_ENABLE << XOSC_CTRL_ENABLE_LSB; 

    // Wait for XOSC to be stable
    while(!(xosc_hw->status & XOSC_STATUS_STABLE_BITS));
}

/* largely inspired by
 * from pico-sdk/src/rp2_common/hardware_pll/pll.c */
void pll_init(pll_hw_t * pll, 
		      const uint32_t refdiv,
			  const uint32_t vco_freq,
			  const uint32_t post_div1,
			  const uint32_t post_div2)
{
    // Turn off PLL in case it is already running
    pll->pwr = 0xffffffff; //p.256
    pll->fbdiv_int = 0;    //p.257

    const uint32_t ref_mhz = XOSC_MHZ / refdiv;
    pll->cs = refdiv; //p.256

    // What are we multiplying the reference clock by to get the vco freq
    // (The regs are called div, because you divide the vco output and compare it to the refclk)
    const uint32_t fbdiv = vco_freq / (ref_mhz * MHZ);

//    // fbdiv
//    assert(fbdiv >= 16 && fbdiv <= 320);
//
//    // Check divider ranges
//    assert((post_div1 >= 1 && post_div1 <= 7) && (post_div2 >= 1 && post_div2 <= 7));
//
//    // post_div1 should be >= post_div2
//    // from appnote page 11
//    // postdiv1 is designed to operate with a higher input frequency
//    // than postdiv2
//    assert(post_div2 <= post_div1);
//
//    // Check that reference frequency is no greater than vco / 16
//    assert(ref_mhz <= (vco_freq / 16));

    // Put calculated value into feedback divider
    pll->fbdiv_int = fbdiv;

    // Turn on PLL
    const uint32_t power = PLL_PWR_PD_BITS | // Main power
                           PLL_PWR_VCOPD_BITS; // VCO Power

    pll->pwr &= ~power;

    // Wait for PLL to lock
    while (!(pll->cs & PLL_CS_LOCK_BITS)) {};

    // Set up post dividers - div1 feeds into div2 so if div1 is 5 and div2 is 2 then you get a divide by 10
    const uint32_t pdiv = (post_div1 << PLL_PRIM_POSTDIV1_LSB) |
                          (post_div2 << PLL_PRIM_POSTDIV2_LSB);
    pll->prim = pdiv;

    // Turn on post divider
    pll->pwr &= ~PLL_PWR_POSTDIVPD_BITS;
}

int clock_configure(enum clock_index clk_index, 
		            uint32_t src,
					uint32_t auxsrc, 
					uint32_t src_freq, 
					uint32_t freq) 
{
    uint32_t div;

    //assert(src_freq >= freq);

    if (freq > src_freq)
        return 0;

    // Div register is 24.8 int.frac divider so multiply by 2^8 (left shift by 8)
    div = (uint32_t) (((uint64_t) src_freq << 8) / freq);

    clock_hw_t *clock = &clocks_hw->clk[clk_index];

    // If increasing divisor, set divisor before source. Otherwise set source
    // before divisor. This avoids a momentary overspeed when e.g. switching
    // to a faster source and increasing divisor to compensate.
    if (div > clock->div) {
        clock->div = div;
    }
    // If switching a glitchless slice (ref or sys) to an aux source, switch
    // away from aux *first* to avoid passing glitches when changing aux mux.
    // Assume (!!!) glitchless source 0 is no faster than the aux source.
	
    const int has_glitchless_mux = (clk_index == clk_sys) || (clk_index == clk_ref) ;

    if (has_glitchless_mux && src == CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX) {
        clock->ctrl &= ~CLOCKS_CLK_REF_CTRL_SRC_BITS;
        while (!(clock->selected & 1u)) {}
    }
    // If no glitchless mux, cleanly stop the clock to avoid glitches
    // propagating when changing aux mux. Note it would be a really bad idea
    // to do this on one of the glitchless clocks (clk_sys, clk_ref).
    else {

        clock->ctrl &= ~CLOCKS_CLK_GPOUT0_CTRL_ENABLE_BITS;
        //if (configured_freq[clk_index] > 0) {
        //    // Delay for 3 cycles of the target clock, for ENABLE propagation.
        //    // Note XOSC_COUNT is not helpful here because XOSC is not
        //    // necessarily running, nor is timer... so, 3 cycles per loop:
        //    uint delay_cyc = configured_freq[clk_sys] / configured_freq[clk_index] + 1;
        //    asm volatile (
        //        ".syntax unified \n\t"
        //        "1: \n\t"
        //        "subs %0, #1 \n\t"
        //        "bne 1b"
        //        : "+r" (delay_cyc)
        //    );
        //}
    }

    // Set aux mux first, and then glitchless mux if this clock has one
	uint32_t tmp = clock->ctrl & ~CLOCKS_CLK_SYS_CTRL_AUXSRC_BITS;
	tmp |= auxsrc << CLOCKS_CLK_SYS_CTRL_AUXSRC_LSB;
    clock->ctrl = tmp;

    if (has_glitchless_mux) {
		tmp = clock->ctrl & ~CLOCKS_CLK_REF_CTRL_SRC_BITS;
		tmp |= src << CLOCKS_CLK_REF_CTRL_SRC_LSB;
        clock->ctrl = tmp;
        while (!(clock->selected & (1u << src))) {}
    }

    clock->ctrl |= CLOCKS_CLK_GPOUT0_CTRL_ENABLE_BITS;

    // Now that the source is configured, we can trust that the user-supplied
    // divisor is a safe value.
    clock->div = div;

    // Store the configured frequency
    //configured_freq[clk_index] = freq;

    return 1; 
}

/*
 * largely inspired by
 * from pico-sdk/src/rp2_common/hardware_clocks/clocks.c */
void clocks_init(void) {
    // Start tick in watchdog
    //watchdog_start_tick(XOSC_MHZ);

    // Disable resus that may be enabled from previous software
    clocks_hw->resus.ctrl = 0;

    // Enable the xosc
    xosc_init();

    // Before we touch PLLs, switch sys and ref cleanly away from their aux sources.
	clocks_hw->clk[clk_sys].ctrl &= ~ CLOCKS_CLK_SYS_CTRL_SRC_BITS ;
    while (clocks_hw->clk[clk_sys].selected != 0x1) {}
    clocks_hw->clk[clk_ref].ctrl &= ~ CLOCKS_CLK_REF_CTRL_SRC_BITS;
    while (clocks_hw->clk[clk_ref].selected != 0x1) {}

    /// \tag::pll_settings[]
    // Configure PLLs
    //                   REF     FBDIV VCO            POSTDIV
    // PLL SYS: 12 / 1 = 12MHz * 125 = 1500MHZ / 6 / 2 = 125MHz
    // PLL USB: 12 / 1 = 12MHz * 40  = 480 MHz / 5 / 2 =  48MHz
    /// \end::pll_settings[]

	//first reset PLL
    const uint32_t resettedPheripheral = RESETS_RESET_PLL_SYS_BITS | RESETS_RESET_PLL_USB_BITS ;
    resets_hw->reset |= resettedPheripheral ; //got to reset state
    resets_hw->reset &= ~resettedPheripheral; //get out of reset state
    //--- Wait until reset is done
    while ((resettedPheripheral & ~resets_hw->reset_done) != 0) {}

    pll_init(pll_sys_hw, 1, 1500 * MHZ, 6, 2);
    pll_init(pll_usb_hw, 1, 480 * MHZ, 5, 2);

    // Configure clocks
    // CLK_REF = XOSC (12MHz) / 1 = 12MHz
    clock_configure(clk_ref,
                    CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC,
                    0, // No aux mux
                    12 * MHZ,
                    12 * MHZ);

    /// \tag::configure_clk_sys[]
    // CLK SYS = PLL SYS (125MHz) / 1 = 125MHz
    clock_configure(clk_sys,
                    CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
                    CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
                    125 * MHZ,
                    125 * MHZ);
    /// \end::configure_clk_sys[]

    // CLK USB = PLL USB (48MHz) / 1 = 48MHz
    clock_configure(clk_usb,
                    0, // No GLMUX
                    CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                    48 * MHZ,
                    48 * MHZ);

    // CLK ADC = PLL USB (48MHZ) / 1 = 48MHz
    clock_configure(clk_adc,
                    0, // No GLMUX
                    CLOCKS_CLK_ADC_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                    48 * MHZ,
                    48 * MHZ);

    // CLK RTC = PLL USB (48MHz) / 1024 = 46875Hz
    clock_configure(clk_rtc,
                    0, // No GLMUX
                    CLOCKS_CLK_RTC_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                    48 * MHZ,
                    46875);

    // CLK PERI = clk_sys. Used as reference clock for Peripherals. No dividers so just select and enable
    // Normally choose clk_sys or clk_usb
    clock_configure(clk_peri,
                    0,
                    CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
                    125 * MHZ,
                    125 * MHZ);
}

/* from pico-sdk/src/rp2_common/pico_runtime/runtime.c */
void runtime_init(void) {
    // Reset all peripherals to put system into a known state,
    // - except for QSPI pads and the XIP IO bank, as this is fatal if running from flash
    // - and the PLLs, as this is fatal if clock muxing has not been reset on this boot
    const uint32_t peripheralsNotResetted =
      RESETS_RESET_IO_QSPI_BITS |
      RESETS_RESET_PADS_QSPI_BITS |
      RESETS_RESET_PLL_USB_BITS |
      RESETS_RESET_PLL_SYS_BITS
    ;
    resets_hw->reset |= ~ peripheralsNotResetted ;

    // Remove reset from peripherals which are clocked only by clk_sys and
    // clk_ref. Other peripherals stay in reset until we've configured clocks.
    const uint32_t otherPeripherals =
       RESETS_RESET_ADC_BITS |
       RESETS_RESET_RTC_BITS |
       RESETS_RESET_SPI0_BITS |
       RESETS_RESET_SPI1_BITS |
       RESETS_RESET_UART0_BITS |
       RESETS_RESET_UART1_BITS |
       RESETS_RESET_USBCTRL_BITS;
    const uint32_t reset = RESETS_RESET_BITS & ~ otherPeripherals ;
    resets_hw->reset &= ~reset ;
    //--- Wait for reset done
    while ((reset & ~resets_hw->reset_done) != 0) {}

    // pre-init runs really early since we need it even for memcpy and divide!
    // (basically anything in aeabi that uses bootrom)

    // Start and end points of the constructor list,
    // defined by the linker script.
    extern void (*__preinit_array_start)(void);
    extern void (*__preinit_array_end)(void);

    // Call each function in the list.
    // We have to take the address of the symbols, as __preinit_array_start *is*
    // the first function pointer, not the address of it.
    for (void (**p)(void) = &__preinit_array_start; p < &__preinit_array_end; ++p) {
        (*p)();
    }

    // After calling preinit we have enough runtime to do the exciting maths
    // in clocks_init
    clocks_init();

    // Peripheral clocks should now all be running
    resets_hw->reset |= otherPeripherals ;
    resets_hw->reset &= ~ otherPeripherals ;
    while ((otherPeripherals & ~resets_hw->reset_done) != 0) {}

//#if !PICO_IE_26_29_UNCHANGED_ON_RESET
//    // after resetting BANK0 we should disable IE on 26-29
//    hw_clear_alias(padsbank0_hw)->io[26] = hw_clear_alias(padsbank0_hw)->io[27] =
//            hw_clear_alias(padsbank0_hw)->io[28] = hw_clear_alias(padsbank0_hw)->io[29] = PADS_BANK0_GPIO0_IE_BITS;
//#endif
//
//    extern mutex_t __mutex_array_start;
//    extern mutex_t __mutex_array_end;
//
//    // the first function pointer, not the address of it.
//    for (mutex_t *m = &__mutex_array_start; m < &__mutex_array_end; m++) {
//        mutex_init(m);
//    }
//
//#if !(PICO_NO_RAM_VECTOR_TABLE || PICO_NO_FLASH)
//    __builtin_memcpy(ram_vector_table, (uint32_t *) scb_hw->vtor, sizeof(ram_vector_table));
//    scb_hw->vtor = (uintptr_t) ram_vector_table;
//#endif
//
//#ifndef NDEBUG
//    if (__get_current_exception()) {
//        // crap; started in exception handler
//        __asm ("bkpt #0");
//    }
//#endif
//
//#if PICO_USE_STACK_GUARDS
//    // install core0 stack guard
//    extern char __StackBottom;
//    runtime_install_stack_guard(&__StackBottom);
//#endif
//
//    spin_locks_reset();
//    irq_init_priorities();
//    alarm_pool_init_default();
//
//    // Start and end points of the constructor list,
//    // defined by the linker script.
//    extern void (*__init_array_start)(void);
//    extern void (*__init_array_end)(void);
//
//    // Call each function in the list.
//    // We have to take the address of the symbols, as __init_array_start *is*
//    // the first function pointer, not the address of it.
//    for (void (**p)(void) = &__init_array_start; p < &__init_array_end; ++p) {
//        (*p)();
//    }

}

void _exit(__attribute__((unused)) int status) {
}

extern char __StackLimit; /* Set by linker.  */

void *_sbrk(int incr) {
    extern char end; /* Set by linker.  */
    static char *heap_end;
    char *prev_heap_end;

    if (heap_end == 0)
        heap_end = &end;

    prev_heap_end = heap_end;
    char *next_heap_end = heap_end + incr;

    if (__builtin_expect(next_heap_end > (&__StackLimit), 0)) {
#if PICO_USE_OPTIMISTIC_SBRK
        if (heap_end == &__StackLimit) {
//        errno = ENOMEM;
            return (char *) -1;
        }
        next_heap_end = &__StackLimit;
#else
        return (char *) -1;
#endif
    }

    heap_end = next_heap_end;
    return (void *) prev_heap_end;
}

// exit is not useful... no desire to pull in __call_exitprocs
void exit(__attribute__((unused)) int status) {
    //_exit(status);
	while(1) {}
}

void hard_assertion_failure(void) {
    //panic("Hard assert");
	while(1) {}
}
