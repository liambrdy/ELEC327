#include <ti/devices/msp/msp.h>
#include "hw_interface.h"

#define POWER_STARTUP_DELAY (32)

void delay_cycles(uint32_t cycles)
{
    /* this is a scratch register for the compiler to use */
    uint32_t scratch;

    /* There will be a 2 cycle delay here to fetch & decode instructions
     * if branch and linking to this function */

    /* Subtract 2 net cycles for constant offset: +2 cycles for entry jump,
     * +2 cycles for exit, -1 cycle for a shorter loop cycle on the last loop,
     * -1 for this instruction */

    __asm volatile(
#ifdef __GNUC__
        ".syntax unified\n\t"
#endif
        "SUBS %0, %[numCycles], #2; \n"
        "%=: \n\t"
        "SUBS %0, %0, #4; \n\t"
        "NOP; \n\t"
        "BHS  %=b;" /* branches back to the label defined above if number > 0 */
        /* Return: 2 cycles */
        : "=&r"(scratch)
        : [ numCycles ] "r"(cycles));
}


void InitializeLED() {
    // 1. Reset GPIO port (the one(s) for pins that you will use)
    GPIOA->GPRCM.RSTCTL = (GPIO_RSTCTL_KEY_UNLOCK_W | GPIO_RSTCTL_RESETSTKYCLR_CLR | GPIO_RSTCTL_RESETASSERT_ASSERT);

    // 2. Enable power on LED GPIO port
    GPIOA->GPRCM.PWREN = (GPIO_PWREN_KEY_UNLOCK_W | GPIO_PWREN_ENABLE_ENABLE);

    delay_cycles(POWER_STARTUP_DELAY); // delay to enable GPIO to turn on and reset

    // Neopixel is connected to PA0
    IOMUX->SECCFG.PINCM[IOMUX_PINCM25] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
    GPIOA->DOUT31_0 = 0x1000000; // default to zero
    GPIOA->DOESET31_0 = 0x1000000;

}


void InitializeTimerG0() {

    /* Timer module and Sleep Mode Initialization */
    // 1. Step 1 is always to reset and enable
    TIMA0->GPRCM.RSTCTL = (GPIO_RSTCTL_KEY_UNLOCK_W | GPIO_RSTCTL_RESETSTKYCLR_CLR | GPIO_RSTCTL_RESETASSERT_ASSERT);
    TIMA0->GPRCM.PWREN = (GPIO_PWREN_KEY_UNLOCK_W | GPIO_PWREN_ENABLE_ENABLE);
    delay_cycles(16); // delay to enable module to turn on and reset

    // 2. Step 2 is to choose the desired clock. We want UPCLK (LFCLK) so we can use a LPM
    // TIMA0->CLKSEL = GPTIMER_CLKSEL_BUSCLK_SEL_ENABLE;
    TIMA0->CLKSEL = GPTIMER_CLKSEL_LFCLK_SEL_ENABLE;

    // 3. By default, the timer counts down to zero and then stops. Configure it to repeat.
    TIMA0->COUNTERREGS.CTRCTL = GPTIMER_CTRCTL_REPEAT_REPEAT_1;

    // 4. Enable timer interrupt when we reach 0
    TIMA0->CPU_INT.IMASK |= GPTIMER_CPU_INT_IMASK_Z_SET;

    // 5. Enable the clock input to the timer. (The timer itself is still not enabled!)
    TIMA0->COMMONREGS.CCLKCTL = GPTIMER_CCLKCTL_CLKEN_ENABLED;
    // 6. Set sleep to be STANDBY (Rev 2025 TRM p. 227, also 2.4.2 Operating Mode Selection)
    SYSCTL->SOCLOCK.PMODECFG = SYSCTL_PMODECFG_DSLEEP_STANDBY;
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    // Set sleep to be STANDBY1 (Rev 2025 TRM Table 2-9. MSPM0Gxx ULPCLK by Operating Mode)
    SYSCTL->SOCLOCK.MCLKCFG |= SYSCTL_MCLKCFG_STOPCLKSTBY_ENABLE;

}

inline void SetTimerG0Delay(uint16_t delay) {
    TIMA0->COUNTERREGS.LOAD = delay; // This will load as soon as timer is enabled
}

inline void EnableTimerG0() {
    TIMA0->COUNTERREGS.CTRCTL |= (GPTIMER_CTRCTL_EN_ENABLED);
    NVIC_EnableIRQ(TIMA0_INT_IRQn); // Enable the timer interrupt
}


// The function needs to be put into the interrupt table!!!!
void TIMA0_IRQHandler(void)
{
    // This wakes up the processor!
    switch (TIMA0->CPU_INT.IIDX) {
        case GPTIMER_CPU_INT_IIDX_STAT_Z: // Counted down to zero event.
            // If we wanted to execute code in the ISR, it would go here.
            break;
        default:
            break;
    }
}

