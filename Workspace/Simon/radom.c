/*
 * simon_random.h
 *
 * Random number generation code for the simon game.
 * Provides for two functions, rand() and srand().
 * 
 * unsigned int rand() returns a pseudorandom number in the range of 0-3
 * void srand(int seed) is used to initialize (or reset) the random number generation
 *
 *  Created on: Mar 9, 2022
 *      Author: ckemere
 */
/*  */
// -------------------------------------
#include "random.h"

#include <ti/devices/msp/msp.h>
#include "delay.h"

uint16_t lfsr; // private variable which holds the state of the random number generation system

void srand(uint16_t seed) {
    int i;
    if (seed == 0)
        lfsr = 1;
    else
        lfsr = seed;

    // Churn the random number generator enough times to scramble the state
    // This is helpful if initializers are often small values, since rand()
    // is grabbing the least significant bits and the pseudorandom sequence
    // gets built from the most significant side.
    for (i = 0; i < 32; i++)
        rand();
}

uint16_t rand() {
    // Generates a random number between 0-3
    unsigned int bit;
    bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5))  & 0x01;
    lfsr = (lfsr >> 1) | (bit << 15);
    bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5))  & 0x01;
    lfsr = (lfsr >> 1) | (bit << 15);
    return lfsr & 0x03;
}

__STATIC_INLINE void update_reg(
    volatile uint32_t *reg, uint32_t val, uint32_t mask)
{
    uint32_t tmp;

    tmp  = *reg;
    tmp  = tmp & ~mask;
    *reg = tmp | (val & mask);
}

uint16_t GenerateSeed()
{
    uint32_t randomNumber = 0;
    // Power on and initialize TRNG
    TRNG->GPRCM.RSTCTL = TRNG_RSTCTL_RESETASSERT_ASSERT | TRNG_RSTCTL_KEY_UNLOCK_W;
    TRNG->GPRCM.PWREN = TRNG_PWREN_KEY_UNLOCK_W | TRNG_PWREN_ENABLE_ENABLE;
    delay_cycles(POWER_STARTUP_DELAY);

    TRNG->CLKDIVIDE = (uint32_t) TRNG_CLKDIVIDE_RATIO_DIV_BY_2;

    // Send "Normal Function" command and then wait for it to complete
    update_reg(&TRNG->CTL, (uint32_t) TRNG_CTL_CMD_NORM_FUNC, TRNG_CTL_CMD_MASK);
    while (!((TRNG->CPU_INT.RIS & TRNG_RIS_IRQ_CMD_DONE_MASK) ==
            TRNG_RIS_IRQ_CMD_DONE_SET));
    TRNG->CPU_INT.ICLR = TRNG_IMASK_IRQ_CMD_DONE_MASK; // clear interrupt status

    update_reg(&TRNG->CTL,
        ((uint32_t) 0x3 << TRNG_CTL_DECIM_RATE_OFS), // set clock to decimate by 4
        TRNG_CTL_DECIM_RATE_MASK);

    /* Setup and start a capture, then wait for the result */
    while (!((TRNG->CPU_INT.RIS & TRNG_RIS_IRQ_CAPTURED_RDY_MASK) ==
            TRNG_RIS_IRQ_CAPTURED_RDY_SET));
    TRNG->CPU_INT.ICLR = TRNG_IMASK_IRQ_CAPTURED_RDY_MASK; // clear interrupt status
    randomNumber = TRNG->DATA_CAPTURE; // here is our random number!!!

    /* Power off the TRNG peripheral */
    TRNG->GPRCM.PWREN = TRNG_PWREN_KEY_UNLOCK_W | TRNG_PWREN_ENABLE_DISABLE;

    return (uint16_t)(randomNumber ^ (randomNumber >> 16));
}