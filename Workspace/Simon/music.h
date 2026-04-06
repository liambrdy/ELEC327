
#ifndef music_include
#define music_include


#define C5 523.252f
#define D5 587.32f
#define E5 659.256f
#define G5 783.99f
#define A5 880.0f
#define F5   698.456f
#define FS5  739.989f
#define GS5  830.609f
#define B4 493.883f


#define MCLK_FREQUENCY 8000000.0  // Use .0 to ensure floating point math

// Helper macro for Load Value calculation
// We subtract 1 from the final value because we are always counting from 0!
// We add 0.5 to ensure rounding rather than truncation in the divide.
#define CALC_LOAD(freq) ((uint16_t)((MCLK_FREQUENCY / (freq)) + 0.5) - 1)

#define C5_LOAD    CALC_LOAD(C5)
#define D5_LOAD    CALC_LOAD(D5)
#define E5_LOAD    CALC_LOAD(E5)
#define G5_LOAD    CALC_LOAD(G5)
#define A5_LOAD    CALC_LOAD(A5)
#define F5_LOAD   CALC_LOAD(F5)
#define FS5_LOAD  CALC_LOAD(FS5)
#define GS5_LOAD  CALC_LOAD(GS5)
#define B4_LOAD CALC_LOAD(B4)

#endif /* music_include */

/*
 *
 * Copyright (c) 2026, Caleb Kemere
 * All rights reserved, see LICENSE.md
 *
 */