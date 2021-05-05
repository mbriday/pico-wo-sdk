/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _HARDWARE_ADDRESS_MAPPED_H
#define _HARDWARE_ADDRESS_MAPPED_H

#include <stddef.h> //offsetof
#ifndef static_assert
  #define static_assert _Static_assert
#endif
#include "hardware/regs/addressmap.h"

/** \file address_mapped.h
 *  \defgroup hardware_base hardware_base
 *
 *  Low-level types and (atomic) accessors for memory-mapped hardware registers
 *
 *  `hardware_base` defines the low level types and access functions for memory mapped hardware registers. It is included
 *  by default by all other hardware libraries.
 *
 *  The following register access typedefs codify the access type (read/write) and the bus size (8/16/32) of the hardware register.
 *  The register type names are formed by concatenating one from each of the 3 parts A, B, C

 *   A    | B | C | Meaning
 *  ------|---|---|--------
 *  io_   |   |   | A Memory mapped IO register
 *  &nbsp;|ro_|   | read-only access
 *  &nbsp;|rw_|   | read-write access
 *  &nbsp;|wo_|   | write-only access (can't actually be enforced via C API)
 *  &nbsp;|   |  8| 8-bit wide access
 *  &nbsp;|   | 16| 16-bit wide access
 *  &nbsp;|   | 32| 32-bit wide access
 *
 *  When dealing with these types, you will always use a pointer, i.e. `io_rw_32 *some_reg` is a pointer to a read/write
 *  32 bit register that you can write with `*some_reg = value`, or read with `value = *some_reg`.
 *
 *  RP2040 hardware is also aliased to provide atomic setting, clear or flipping of a subset of the bits within
 *  a hardware register so that concurrent access by two cores is always consistent with one atomic operation
 *  being performed first, followed by the second.
 *
 *  See hw_set_bits(), hw_clear_bits() and hw_xor_bits() provide for atomic access via a pointer to a 32 bit register
 *
 *  Additionally given a pointer to a structure representing a piece of hardware (e.g. `dma_hw_t *dma_hw` for the DMA controller), you can
 *  get an alias to the entire structure such that writing any member (register) within the structure is equivalent
 *  to an atomic operation via hw_set_alias(), hw_clear_alias() or hw_xor_alias()...
 *
 *  For example `hw_set_alias(dma_hw)->inte1 = 0x80;` will set bit 7 of the INTE1 register of the DMA controller,
 *  leaving the other bits unchanged.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define check_hw_layout(type, member, offset) static_assert(offsetof(type, member) == (offset), "hw offset mismatch")
#define check_hw_size(type, size) static_assert(sizeof(type) == (size), "hw size mismatch")

typedef volatile uint32_t io_rw_32;
typedef const volatile uint32_t io_ro_32;
typedef volatile uint32_t io_wo_32;
typedef volatile uint16_t io_rw_16;
typedef const volatile uint16_t io_ro_16;
typedef volatile uint16_t io_wo_16;
typedef volatile uint8_t io_rw_8;
typedef const volatile uint8_t io_ro_8;
typedef volatile uint8_t io_wo_8;

typedef volatile uint8_t *const ioptr;
typedef ioptr const const_ioptr;

// Untyped conversion alias pointer generation macros
#define hw_set_alias_untyped(addr) ((void *)(REG_ALIAS_SET_BITS | (uintptr_t)(addr)))
#define hw_clear_alias_untyped(addr) ((void *)(REG_ALIAS_CLR_BITS | (uintptr_t)(addr)))
#define hw_xor_alias_untyped(addr) ((void *)(REG_ALIAS_XOR_BITS | (uintptr_t)(addr)))

// Typed conversion alias pointer generation macros
#define hw_set_alias(p) ((typeof(p))hw_set_alias_untyped(p))
#define hw_clear_alias(p) ((typeof(p))hw_clear_alias_untyped(p))
#define hw_xor_alias(p) ((typeof(p))hw_xor_alias_untyped(p))

/*! \brief Atomically set the specified bits to 1 in a HW register
 *  \ingroup hardware_base
 *
 * \param addr Address of writable register
 * \param mask Bit-mask specifying bits to set
 */
inline static void hw_set_bits(io_rw_32 *addr, uint32_t mask) {
    *(io_rw_32 *) hw_set_alias_untyped((volatile void *) addr) = mask;
}

/*! \brief Atomically clear the specified bits to 0 in a HW register
 *  \ingroup hardware_base
 *
 * \param addr Address of writable register
 * \param mask Bit-mask specifying bits to clear
 */
inline static void hw_clear_bits(io_rw_32 *addr, uint32_t mask) {
    *(io_rw_32 *) hw_clear_alias_untyped((volatile void *) addr) = mask;
}

/*! \brief Atomically flip the specified bits in a HW register
 *  \ingroup hardware_base
 *
 * \param addr Address of writable register
 * \param mask Bit-mask specifying bits to invert
 */
inline static void hw_xor_bits(io_rw_32 *addr, uint32_t mask) {
    *(io_rw_32 *) hw_xor_alias_untyped((volatile void *) addr) = mask;
}

/*! \brief Set new values for a sub-set of the bits in a HW register
 *  \ingroup hardware_base
 *
 * Sets destination bits to values specified in \p values, if and only if corresponding bit in \p write_mask is set
 *
 * Note: this method allows safe concurrent modification of *different* bits of
 * a register, but multiple concurrent access to the same bits is still unsafe.
 *
 * \param addr Address of writable register
 * \param values Bits values
 * \param write_mask Mask of bits to change
 */
inline static void hw_write_masked(io_rw_32 *addr, uint32_t values, uint32_t write_mask) {
    hw_xor_bits(addr, (*addr ^ values) & write_mask);
}

#ifdef __cplusplus
}
#endif

#endif
