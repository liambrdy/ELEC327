#include "buttons.h"
#include <ti/devices/msp/msp.h>

#define CFG_INPUT(PINCM_IDX, PF) \
    IOMUX->SECCFG.PINCM[(PINCM_IDX)] = \
        IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM_INENA_ENABLE | \
        IOMUX_PINCM_PIPU_ENABLE  | (PF)

void buttons_init(void) {
    CFG_INPUT(BTN_UP_PINCM,    BTN_UP_PF);
    CFG_INPUT(BTN_DOWN_PINCM,  BTN_DOWN_PF);
    CFG_INPUT(BTN_LEFT_PINCM,  BTN_LEFT_PF);
    CFG_INPUT(BTN_RIGHT_PINCM, BTN_RIGHT_PF);
    CFG_INPUT(BTN_A_PINCM,     BTN_A_PF);
    CFG_INPUT(BTN_B_PINCM,     BTN_B_PF);
}

uint32_t hw_buttons_read(void) {
    uint32_t result = 0;
    if (!(BTN_UP_PORT->DIN31_0    & (1u << BTN_UP_DIO)))    result |= BTN_MASK_UP;
    if (!(BTN_DOWN_PORT->DIN31_0  & (1u << BTN_DOWN_DIO)))  result |= BTN_MASK_DOWN;
    if (!(BTN_LEFT_PORT->DIN31_0  & (1u << BTN_LEFT_DIO)))  result |= BTN_MASK_LEFT;
    if (!(BTN_RIGHT_PORT->DIN31_0 & (1u << BTN_RIGHT_DIO))) result |= BTN_MASK_RIGHT;
    if (!(BTN_A_PORT->DIN31_0     & (1u << BTN_A_DIO)))     result |= BTN_MASK_A;
    if (!(BTN_B_PORT->DIN31_0     & (1u << BTN_B_DIO)))     result |= BTN_MASK_B;
    return result;
}
