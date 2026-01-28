#ifndef initialize_leds_include
#define initialize_leds_include

void InitializeGPIO();

typedef struct led {
    int gpio;
    int pin;
} led;

extern led leds[];

#endif /* initialize_leds_include */
