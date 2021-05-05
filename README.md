# Pico BareMetal example without SDK

This repository aims to program baremetal apps on RPI Pico (RP2040) without any SDK, i.e. only with register access.

Stricly minimum parts (clocks config, runtime) are taken from the sdk.

In `sys`:
 * `cmake` dir to build the stuff
 * `rp2040` dir that gets:
	 * `hardware_regs` and `hardware_structs` form the SDK (register defs).
	 * `hardware` few required header files to make it work. 
 * `boot2_w25q080_2_padded_checksum.s` that contains the first 256 bytes to configure the external flash for the PICO. (first stage boot)
 * * `crt0.S` from the SDK that starts the MCU (second stage boot)
 * `runtime.c` that inits the MCU before `main()` (third stage boot)
 * `memmap_default.ld` is the default linker script.

One header file is `rp2040.h` that includes all the reg defs.
# Examples
## Blink
A minimal example that blinks led. First example after a successful startup.
