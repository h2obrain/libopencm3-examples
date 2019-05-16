/*
 * This include file describes the functions exported by clock.c
 */
#ifndef __CLOCK_H
#define __CLOCK_H

#include <stdint.h>

#include <libopencm3/stm32/rcc.h>

#define HSE_FREQUENCY_MHZ 25
#define CLOCK_SETUP rcc_3v3[RCC_CLOCK_3V3_216MHZ]
//#define CLOCK_SETUP rcc_3v3[RCC_CLOCK_3V3_168MHZ]
//#define CLOCK_SETUP rcc_3v3[RCC_CLOCK_3V3_120MHZ]

/*
 * Definitions for functions being abstracted out
 */
void msleep(uint32_t);
uint64_t mtime(void);
void clock_setup(void);

/*
 * Delay functions which are not using clock :)
 */
#define CYCLES_PER_LOOP 3
static inline void wait_cycles(uint32_t n)
{
	uint32_t l = n/CYCLES_PER_LOOP;
	if (!l) return;
	__asm__ __volatile__("0:" "SUBS %[count], 1;" "BNE 0b;":[count]"+r"(l));
}

static inline void msleep_loop(uint32_t ms)
{
	wait_cycles(CLOCK_SETUP.ahb_frequency / 1000 * ms);
}


#endif /* generic header protector */

