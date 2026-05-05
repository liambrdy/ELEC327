#ifndef SD_H
#define SD_H

#include <stdint.h>
#include <stdbool.h>

#define SD_CS_PINCM   IOMUX_PINCM43
#define SD_CS_PF      IOMUX_PINCM43_PF_GPIOB_DIO17
#define SD_CS_PORT    GPIOB
#define SD_CS_PIN     (1UL << 17)

#define SD_CD_PINCM       IOMUX_PINCM32
#define SD_CD_PF          IOMUX_PINCM32_PF_GPIOB_DIO15
#define SD_CD_PORT        GPIOB
#define SD_CD_PIN         (1UL << 15)
#define SD_CD_ACTIVE_LOW  0

void    sd_cd_init(void);
bool    sd_init(void);
bool    sd_card_present(void);
bool    sd_read_block(uint32_t block, uint8_t *buf);

extern uint8_t sd_last_r1;

#endif /* SD_H */
