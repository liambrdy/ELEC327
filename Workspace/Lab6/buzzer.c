
#include "buzzer.h"
#include "delay.h"
#include <ti/devices/msp/msp.h>

#include <math.h>

volatile uint32_t phaseAcc = 0;
volatile uint32_t phaseInc = 0;
uint16_t sineTable[TABLE_SIZE];

volatile bool buzzerDisabled = true;

void InitSineTable(void) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        float val = sinf(2.0f * M_PI * i / TABLE_SIZE);
        sineTable[i] = (uint16_t)((val + 1.0f) * 2047.5f); // scale to 8-bit DAC
    }
}

void SetBuzzerFrequency(float freq) {
    if (freq == 0) {
        buzzerDisabled = true;
        return;
    }
    
    uint32_t newInc = (uint32_t)((freq / SAMPLE_RATE) * 4294967296.0f);
    if (newInc == 0) newInc = 1;
    
    buzzerDisabled = false;
    phaseAcc = 0;
    phaseInc = newInc;
}

void EnableBuzzer(void) {
    for (int i = 0; i < 4; i++) {
        DAC0->DATA0 = 2048;
    }

    DAC0->CPU_INT.IMASK |= DAC12_CPU_INT_IMASK_FIFOEMPTYIFG_SET;

    NVIC_SetPriority(DAC0_INT_IRQn, 0);
    NVIC_EnableIRQ(DAC0_INT_IRQn);
    DAC0->CTL0 |= DAC12_CTL0_ENABLE_SET;
}

void DisableBuzzer(void) {
    DAC0->CTL0 &= ~DAC12_CTL0_ENABLE_SET;
    NVIC_ClearPendingIRQ(DAC0_INT_IRQn);
    NVIC_DisableIRQ(DAC0_INT_IRQn);
}

void InitializeBuzzer(void) {
    DAC0->GPRCM.RSTCTL = (DAC12_RSTCTL_KEY_UNLOCK_W | DAC12_RSTCTL_RESETSTKYCLR_CLR | DAC12_RSTCTL_RESETASSERT_ASSERT);
    DAC0->GPRCM.PWREN = (DAC12_PWREN_KEY_UNLOCK_W | DAC12_PWREN_ENABLE_ENABLE);
    delay_cycles(POWER_STARTUP_DELAY);

    SYSCTL->SOCLOCK.GENCLKEN |= SYSCTL_GENCLKEN_MFPCLKEN_ENABLE;

    DAC0->CTL0 |= DAC12_CTL0_RES__12BITS | DAC12_CTL0_DFM_BINARY;
    DAC0->CTL1 |= DAC12_CTL1_OPS_OUT0 | DAC12_CTL1_AMPEN_ENABLE | DAC12_CTL1_REFSN_VSSA | DAC12_CTL1_REFSP_VDDA;
    DAC0->CTL2 |= DAC12_CTL2_FIFOEN_SET | DAC12_CTL2_FIFOTRIGSEL_STIM;
    DAC0->CTL3 |= DAC12_CTL3_STIMCONFIG__100KSPS | DAC12_CTL3_STIMEN_SET;

    InitSineTable();
}

void DAC0_IRQHandler(void) {
    switch (DAC0->CPU_INT.IIDX) {
        case DAC12_CPU_INT_IIDX_STAT_FIFOEMPTYIFG: {
            for (int i = 0; i < 4; i++) {
                if (!buzzerDisabled) {
                    uint32_t idx = phaseAcc >> PHASE_SHIFT;
                    int32_t sample = sineTable[idx];
                    DAC0->DATA0 = sample;
                    // DAC0->DATA0 = (idx < (TABLE_SIZE / 2)) ? 4095 : 0;
                    phaseAcc += phaseInc;
                } else {
                    DAC0->DATA0 = 2048;
                }
            }
        } break;

        default: break;
    }
}