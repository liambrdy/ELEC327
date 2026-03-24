#ifndef buzzer_include
#define buzzer_include

#include <stdbool.h>
#include <stdint.h>

#define TABLE_SIZE 256
#define TABLE_SIZE_BITS 8
#define PHASE_SHIFT (32 - TABLE_SIZE_BITS)

#define SAMPLE_RATE 100000

extern uint16_t sineTable[TABLE_SIZE];
extern volatile uint32_t phaseAcc;
extern volatile uint32_t phaseInc;

extern volatile bool buzzerDisabled;

void InitializeBuzzer(void);

void SetBuzzerFrequency(float freq);
void EnableBuzzer(void);
void DisableBuzzer(void);

void InitSineTable(void);

#endif // buzzer_include