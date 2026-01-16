#include "initialize_leds.h"
#include "delay.h"
#include <ti/devices/msp/msp.h>

#define POWER_STARTUP_DELAY (16)

led minLeds[] = {
    { 0, 1 }, // 12
    { 26, 59 }, // 1
    { 24, 54 },
    { 22, 47 },
    { 18, 40 },
    { 16, 38 },
    { 14, 36 },
    { 12, 34 },
    { 10, 21 },
    { 8, 19 },
    { 6, 11 },
    { 28, 3 }, // 11
};

led secLeds[] = {
    { 27, 60 }, // 12
    { 25, 55 }, // 1
    { 23, 53 },
    { 21, 46 },
    { 17, 39 },
    { 15, 37 },
    { 13, 35 },
    { 11, 22 },
    { 9, 20 },
    { 7, 14 },
    { 5, 10 },
    { 1, 2 }, // 11
};

void InitializeGPIO() {
    GPIOA->GPRCM.RSTCTL = (GPIO_RSTCTL_KEY_UNLOCK_W | GPIO_RSTCTL_RESETSTKYCLR_CLR | GPIO_RSTCTL_RESETASSERT_ASSERT);
    GPIOA->GPRCM.PWREN = (GPIO_PWREN_KEY_UNLOCK_W | GPIO_PWREN_ENABLE_ENABLE);

    GPIOB->GPRCM.RSTCTL = (GPIO_RSTCTL_KEY_UNLOCK_W | GPIO_RSTCTL_RESETSTKYCLR_CLR | GPIO_RSTCTL_RESETASSERT_ASSERT);
    GPIOB->GPRCM.PWREN = (GPIO_PWREN_KEY_UNLOCK_W | GPIO_PWREN_ENABLE_ENABLE);

    delay_cycles(POWER_STARTUP_DELAY);

    uint32_t enabledPins = 0;

    for (int i = 0; i < 12; i++) {
        led l = minLeds[i];
        IOMUX->SECCFG.PINCM[(IOMUX_PINCM1 + l.pin - 1)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
        enabledPins |= (1 << l.gpio);

        l = secLeds[i];
        IOMUX->SECCFG.PINCM[(IOMUX_PINCM1 + l.pin - 1)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
        enabledPins |= (1 << l.gpio);
    }

    GPIOA->DOUTSET31_0 = enabledPins;
    GPIOA->DOESET31_0 = enabledPins;

    IOMUX->SECCFG.PINCM[(IOMUX_PINCM43)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
    GPIOB->DOUTSET31_0 = 0x0;
    GPIOB->DOESET31_0 = 1 << 17;
}
