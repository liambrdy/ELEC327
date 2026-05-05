#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>
#include <ti/devices/msp/msp.h>

/*
 * Hardware button configuration — change these to match your PCB wiring.
 *
 * Each button should be wired between the GPIO pin and GND with an internal
 * pull-up enabled (or an external 10kΩ pull-up to 3.3V).
 * Button pressed = pin pulled LOW = reading 0.
 *
 * For each button you need:
 *   PORT  : GPIOA or GPIOB
 *   DIO   : the DIO bit number (e.g. 5 for PA5)
 *   PINCM : IOMUX PINCM index from the datasheet pin-mux table
 *   PF    : GPIO input function constant for that PINCM index
 *
 * Example layout (28-pin MSPM0G3507) — adjust to your schematic:
 *   BTN_UP    → PA18  PINCM39  IOMUX_PINCM39_PF_GPIOA_DIO18
 *   BTN_DOWN  → PA17  PINCM38  IOMUX_PINCM38_PF_GPIOA_DIO17
 *   BTN_LEFT  → PA16  PINCM36  IOMUX_PINCM36_PF_GPIOA_DIO16
 *   BTN_RIGHT → PA15  PINCM35  IOMUX_PINCM35_PF_GPIOA_DIO15
 *   BTN_A     → PA14  PINCM30  IOMUX_PINCM30_PF_GPIOA_DIO14
 *
 * NOTE: the SD card CS default (sd.h) uses PA3.  Make sure no button
 * shares a pin with the SD CS, TFT CS/DC/RST, or SPI lines.
 */

#define BTN_UP_PORT    GPIOA
#define BTN_UP_DIO     23
#define BTN_UP_PINCM   IOMUX_PINCM53
#define BTN_UP_PF      IOMUX_PINCM53_PF_GPIOA_DIO23

#define BTN_DOWN_PORT  GPIOA
#define BTN_DOWN_DIO   17
#define BTN_DOWN_PINCM IOMUX_PINCM39
#define BTN_DOWN_PF    IOMUX_PINCM39_PF_GPIOA_DIO17

#define BTN_LEFT_PORT  GPIOA
#define BTN_LEFT_DIO   25
#define BTN_LEFT_PINCM IOMUX_PINCM53
#define BTN_LEFT_PF    IOMUX_PINCM55_PF_GPIOA_DIO25

#define BTN_RIGHT_PORT GPIOA
#define BTN_RIGHT_DIO  24
#define BTN_RIGHT_PINCM IOMUX_PINCM54
#define BTN_RIGHT_PF   IOMUX_PINCM54_PF_GPIOA_DIO24

#define BTN_A_PORT     GPIOA
#define BTN_A_DIO      18
#define BTN_A_PINCM    IOMUX_PINCM40
#define BTN_A_PF       IOMUX_PINCM40_PF_GPIOA_DIO18

#define BTN_B_PORT     GPIOA
#define BTN_B_DIO      17
#define BTN_B_PINCM    IOMUX_PINCM39
#define BTN_B_PF       IOMUX_PINCM39_PF_GPIOA_DIO17

/* Bitmask constants — must match game_api.h exactly. */
#define BTN_MASK_UP    1u
#define BTN_MASK_DOWN  2u
#define BTN_MASK_LEFT  4u
#define BTN_MASK_RIGHT 8u
#define BTN_MASK_A     16u
#define BTN_MASK_B     32u

/* Configure all button GPIO pins as inputs with pull-ups. */
void buttons_init(void);

/* Return a bitmask of currently pressed buttons (BTN_MASK_* bits set = pressed). */
uint32_t hw_buttons_read(void);

#endif /* BUTTONS_H */
