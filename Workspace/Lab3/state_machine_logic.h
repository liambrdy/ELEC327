#ifndef state_machine_logic_include
#define state_machine_logic_include

#define PWM_FREQUENCY 5000
#define DELAY (32000 / PWM_FREQUENCY)
#define PWM_PERIOD_US (1000000 / PWM_FREQUENCY)

typedef enum clock_mode {
    NORMAL,
    HOUR_SET,
    MINUTE_SET,
    BRIGHTNESS_SET,
    CLOCK_MODE_MAX,
} clock_mode;

// State struct
typedef struct time_state {
    int hour;
    int minute;
    int pwm;
    int brightness;

    int button_press_length;

    clock_mode mode;
} time_state;

time_state GetNextState(time_state current_state, int button_down);
int GetStateOutput(time_state current_state);

#endif /* state_machine_logic_include */
