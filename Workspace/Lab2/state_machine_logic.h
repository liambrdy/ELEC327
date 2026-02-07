#ifndef state_machine_logic_include
#define state_machine_logic_include

#define PWM_FREQUENCY 10000
#define DELAY (32000 / PWM_FREQUENCY)

#define PWM_TO_SECOND (32000 / DELAY)

// State struct
typedef struct time_state {
    int seconds;
    int minutes;
    int pwm;
} time_state;

time_state GetNextState(time_state current_state);

int GetStateOutputGPIOA(time_state current_state);

int GetStateOutputGPIOB(time_state current_state);

#endif /* state_machine_logic_include */
