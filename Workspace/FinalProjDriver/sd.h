#ifndef SD_H
#define SD_H

#include <stdint.h>
#include <stdbool.h>

#define SD_CS_PINCM   IOMUX_PINCM36
#define SD_CS_PF      IOMUX_PINCM36_PF_GPIOA_DIO14
#define SD_CS_PORT    GPIOA
#define SD_CS_PIN     (1UL << 14)

#define SD_CD_PINCM       IOMUX_PINCM38
#define SD_CD_PF          IOMUX_PINCM38_PF_GPIOA_DIO16
#define SD_CD_PORT        GPIOA
#define SD_CD_PIN         (1UL << 16)
#define SD_CD_ACTIVE_LOW  0

void    sd_cd_init(void);
bool    sd_init(void);
bool    sd_card_present(void);
bool    sd_read_block(uint32_t block, uint8_t *buf);

extern uint8_t sd_last_r1;

#endif /* SD_H */
