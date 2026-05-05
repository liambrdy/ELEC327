#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>
#include <ti/devices/msp/msp.h>

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

#define BTN_RIGHT_PORT  GPIOA
#define BTN_RIGHT_DIO   24
#define BTN_RIGHT_PINCM IOMUX_PINCM54
#define BTN_RIGHT_PF    IOMUX_PINCM54_PF_GPIOA_DIO24

#define BTN_A_PORT     GPIOA
#define BTN_A_DIO      18
#define BTN_A_PINCM    IOMUX_PINCM40
#define BTN_A_PF       IOMUX_PINCM40_PF_GPIOA_DIO18

#define BTN_B_PORT     GPIOA
#define BTN_B_DIO      17
#define BTN_B_PINCM    IOMUX_PINCM39
#define BTN_B_PF       IOMUX_PINCM39_PF_GPIOA_DIO17

/* must match game_api.h */
#define BTN_MASK_UP    1u
#define BTN_MASK_DOWN  2u
#define BTN_MASK_LEFT  4u
#define BTN_MASK_RIGHT 8u
#define BTN_MASK_A     16u
#define BTN_MASK_B     32u

void buttons_init(void);
uint32_t hw_buttons_read(void);

#endif /* BUTTONS_H */
