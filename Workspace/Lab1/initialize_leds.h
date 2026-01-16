#ifndef initialize_leds_include
#define initialize_leds_include

void InitializeGPIO();

typedef struct led {
    int gpio;
    int pin;
} led;

extern led minLeds[];
extern led secLeds[];

#endif /* initialize_leds_include */
