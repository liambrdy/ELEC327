/*
 * This template file
 */

#include <ti/devices/msp/msp.h>
#include "delay.h"
#include "initialize_leds.h"
#include "state_machine_logic.h"

#define POWER_STARTUP_DELAY (16)

/* This results in approximately 0.5s of delay assuming 32 MHz CPU_CLK */

int main(void)
{
    InitializeGPIO();
    
    time_state state = {}; // initialize state machine

    // Restart and power on timer module
    TIMG0->GPRCM.RSTCTL = (GPIO_RSTCTL_KEY_UNLOCK_W | GPIO_RSTCTL_RESETSTKYCLR_CLR | GPIO_RSTCTL_RESETASSERT_ASSERT);
    TIMG0->GPRCM.PWREN = (GPIO_PWREN_KEY_UNLOCK_W | GPIO_PWREN_ENABLE_ENABLE);
    delay_cycles(POWER_STARTUP_DELAY);

    // Set clock registers
    TIMG0->CLKSEL = GPTIMER_CLKSEL_LFCLK_SEL_ENABLE; // Select LFCLK
    TIMG0->COUNTERREGS.CTRCTL = GPTIMER_CTRCTL_REPEAT_REPEAT_1; // Set clock to repeat
    TIMG0->CPU_INT.IMASK |= GPTIMER_CPU_INT_IMASK_Z_SET; // Interrupt when clock reaches 0
    TIMG0->COMMONREGS.CCLKCTL = GPTIMER_CCLKCTL_CLKEN_ENABLED;

    SYSCTL->SOCLOCK.PMODECFG = SYSCTL_PMODECFG_DSLEEP_STANDBY;
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    SYSCTL->SOCLOCK.MCLKCFG |= SYSCTL_MCLKCFG_STOPCLKSTBY_ENABLE;
    
    TIMG0->COUNTERREGS.LOAD = DELAY; // Set standby delay time
    TIMG0->COUNTERREGS.CTRCTL |= (GPTIMER_CTRCTL_EN_ENABLED);
    
    NVIC_EnableIRQ(TIMG0_INT_IRQn);

    // Functional
    while (1) {
        // update GPIO pins and get next state
        GPIOA->DOUT31_0 = GetStateOutputGPIOA(state);

        state = GetNextState(state);

        __WFI();
    }
}

void TIMG0_IRQHandler(void)
{
    // This wakes up the processor!
    switch (TIMG0->CPU_INT.IIDX) {
        case GPTIMER_CPU_INT_IIDX_STAT_Z: // Counted down to zero event.
            break;
        default:
            break;
    }
}

/*
 * Copyright (c) 2023, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
