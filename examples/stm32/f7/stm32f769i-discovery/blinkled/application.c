/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2019 Oliver Meier <h2obrain@gmail.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#include <libopencmsis/core_cm3.h>

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/exti.h>
#include "clock.h"

/**
 * Simplifying functions
 */

typedef struct {
	uint32_t GPIO;      // base address
	uint32_t GPIO_PINS; // flags
	// config
	uint8_t mode;
	uint8_t pullup_type;
	/*  */
	struct {       // if mode = af or output (i don't think af is needed here, but it's used in many samples, so i leave it..
		uint8_t otype;
		uint8_t ospeed;
	} out_mode;
	struct {        // if mode = af
		uint8_t gpioaf;
	} af_mode;
} pin_setup_t;

/* zero terminated list of pin-configs */
static inline void board_setup_clocks(uint32_t* clocks) {
	while (*clocks) {
		rcc_periph_clock_enable(*clocks);
		clocks++;
	}
	/* some clocks need some time before we can write to the registers
	 * (see errata sheet (it's only 1-2 cpu cycles..)
	 */
	__asm__ volatile ("dsb");
}

/* zero terminated list of pin-configs */
static inline void board_setup_pins(pin_setup_t *pin_configs) {
	while (pin_configs->GPIO) {
		/* mask out pins which are configured later in the array */
		pin_setup_t *pin_configs_following = pin_configs+1;
		while (pin_configs_following->GPIO) {
			if (pin_configs->GPIO == pin_configs_following->GPIO) {
				pin_configs->GPIO_PINS &= ~(pin_configs->GPIO_PINS & pin_configs_following->GPIO_PINS);
			}
			pin_configs_following++;
		}

		/* setup the pin(s) */
		if (pin_configs->GPIO_PINS) {
			gpio_mode_setup(pin_configs->GPIO, pin_configs->mode, pin_configs->pullup_type, pin_configs->GPIO_PINS);
			if ((pin_configs->mode == GPIO_MODE_OUTPUT) || (pin_configs->mode == GPIO_MODE_AF))
				gpio_set_output_options(pin_configs->GPIO, pin_configs->out_mode.otype, pin_configs->out_mode.ospeed, pin_configs->GPIO_PINS);
			if  (pin_configs->mode == GPIO_MODE_AF)
				gpio_set_af(pin_configs->GPIO, pin_configs->af_mode.gpioaf, pin_configs->GPIO_PINS);
		}

		pin_configs++;
	}
}

/**
 * Setup functions
 */
#define LED_LD1 GPIOJ,GPIO13
#define LED_LD2 GPIOJ,GPIO5
#define LED_LD3 GPIOA,GPIO12
#define BUTTON_BLUE GPIOA,GPIO0
#define BUTTON_BLUE_PRESSED() gpio_get(BUTTON_BLUE)
static void pin_setup(void) {
	/* pin-clocks */
	board_setup_clocks((uint32_t[]){
		RCC_GPIOA,
		RCC_GPIOJ,
		0
	});
	/* pins */ \
	board_setup_pins((pin_setup_t[]) {
		/* outputs (LEDs 1-3) */
		{GPIOJ, GPIO5  | GPIO13            , GPIO_MODE_OUTPUT , GPIO_PUPD_NONE     , .out_mode={ GPIO_OTYPE_PP, GPIO_OSPEED_25MHZ  }},
		{GPIOA, GPIO12                     , GPIO_MODE_OUTPUT , GPIO_PUPD_NONE     , .out_mode={ GPIO_OTYPE_PP, GPIO_OSPEED_25MHZ  }},
		/* inputs (blue button) */
		{GPIOA, GPIO0                      , GPIO_MODE_INPUT  , GPIO_PUPD_NONE     , .out_mode={0}},
		{0}
	});

	/* setup blue/user button */
	exti_select_source(EXTI0, GPIOA);
	exti_set_trigger(EXTI0, EXTI_TRIGGER_BOTH);
	exti_enable_request(EXTI0);
	nvic_enable_irq(NVIC_EXTI0_IRQ);
}

/**
 * Function definitions
 */
static void update_led_counter(void);

/**
 * Interrupts
 */

/* Blue button interrupt (EXTI makes not too much sense here :)) */
static bool blue_button_state_changed = false;
void
exti0_isr()
{
	exti_reset_request(EXTI0);
	blue_button_state_changed = true;
	update_led_counter();
}

/**
 * Functions
 */
void update_led_counter() {
	/* little grey code blinker */
	static const uint32_t  gcl[] = {0,1,3,2,6,7,5,4};
	static const uint32_t *gc    = gcl;
	static const uint32_t *gcle  = gcl+sizeof(gcl)/sizeof(gcl[0])-1;
	if (blue_button_state_changed) {
		blue_button_state_changed = false;
		gc = gcl;
	}
	if (BUTTON_BLUE_PRESSED()) {
		gpio_set(LED_LD1);
		gpio_set(LED_LD2);
		gpio_set(LED_LD3);
	} else {
		if (*gc&0b001) gpio_set(LED_LD1);
		else           gpio_clear(LED_LD1);
		if (*gc&0b010) gpio_set(LED_LD2);
		else           gpio_clear(LED_LD2);
		if (*gc&0b100) gpio_set(LED_LD3);
		else           gpio_clear(LED_LD3);
		if (gc==gcle) gc = gcl;
		else          gc++;
	}
}

/**
 * Main loop
 */
int main(void)
{
	/* init timers. */
	clock_setup();
	/* setup pins */
	pin_setup();

	while (1) {
		msleep(1000);
		update_led_counter();
	}
}

