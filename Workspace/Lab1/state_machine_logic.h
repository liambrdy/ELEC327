#ifndef state_machine_logic_include
#define state_machine_logic_include

// State struct
typedef struct time_state {
    int seconds;
    int minutes;
} time_state;

time_state GetNextState(time_state current_state);

int GetStateOutputGPIOA(time_state current_state);

int GetStateOutputGPIOB(time_state current_state);

#endif /* state_machine_logic_include */
