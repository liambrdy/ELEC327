#include "timing.h"
#include "delay.h"
#include <ti/devices/msp/msp.h>

bool timer_wakeup;

inline void SetTimerG0Delay(uint16_t delay) {
    TIMG0->COUNTERREGS.LOAD = delay;
}

inline void EnableTimerG0(void) {
    TIMG0->COUNTERREGS.CTRCTL |= GPTIMER_CTRCTL_EN_ENABLED;
    NVIC_EnableIRQ(TIMG0_INT_IRQn);
}

void InitializeTimerG0(void) {
    TIMG0->GPRCM.RSTCTL = (GPTIMER_RSTCTL_KEY_UNLOCK_W | GPTIMER_RSTCTL_RESETSTKYCLR_CLR | GPTIMER_RSTCTL_RESETASSERT_ASSERT);
    TIMG0->GPRCM.PWREN  = (GPTIMER_PWREN_KEY_UNLOCK_W | GPTIMER_PWREN_ENABLE_ENABLE);
    delay_cycles(POWER_STARTUP_DELAY);

    TIMG0->CLKSEL = GPTIMER_CLKSEL_LFCLK_SEL_ENABLE;
    TIMG0->COUNTERREGS.CTRCTL = GPTIMER_CTRCTL_REPEAT_REPEAT_1;
    TIMG0->CPU_INT.IMASK |= GPTIMER_CPU_INT_IMASK_Z_SET;
    TIMG0->COMMONREGS.CCLKCTL = GPTIMER_CCLKCTL_CLKEN_ENABLED;

    timer_wakeup = false;
}

void TIMG0_IRQHandler(void) {
    switch (TIMG0->CPU_INT.IIDX) {
        case GPTIMER_CPU_INT_IIDX_STAT_Z:
            timer_wakeup = true;
            break;
        default:
            break;
    }
}
