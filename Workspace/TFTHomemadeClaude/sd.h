#ifndef SD_H
#define SD_H

#include <stdint.h>
#include <stdbool.h>

/*
 * SD card SPI driver — shares SPI1 with the TFT.
 * The SD card needs its own CS pin; change the defines below to match
 * your PCB wiring.
 *
 * Steps to find the right values:
 *   1. Pick a free GPIO pin (e.g. PA3).
 *   2. Look up its PINCM index in the MSPM0G3507 datasheet (Table 6-xx).
 *   3. Fill in SD_CS_PINCM, SD_CS_PF, SD_CS_PORT, SD_CS_PIN.
 *
 * The examples below use PA3 (PINCM4).  Change them to your actual pin.
 */
#define SD_CS_PINCM   IOMUX_PINCM43
#define SD_CS_PF      IOMUX_PINCM43_PF_GPIOB_DIO17
#define SD_CS_PORT    GPIOB
#define SD_CS_PIN     (1UL << 17)

/*
 * Card-detect (CD) pin — the mechanical switch inside the SD socket.
 * Change these to match your PCB wiring.
 *
 * This socket floats the pin when a card is inserted and ties it to GND when
 * no card is present.  With the internal pull-up enabled (see sd_init), the
 * pin reads HIGH when a card is present and LOW when absent, so
 * SD_CD_ACTIVE_LOW is 0.
 */
#define SD_CD_PINCM       IOMUX_PINCM32
#define SD_CD_PF          IOMUX_PINCM32_PF_GPIOB_DIO15
#define SD_CD_PORT        GPIOB
#define SD_CD_PIN         (1UL << 15)
#define SD_CD_ACTIVE_LOW  0   /* 0 = card present when pin HIGH (floats → pull-up → HIGH) */

/* Configure the CD pin GPIO input with pull-up.  Must be called before
   sd_card_present() — can happen long before sd_init(). */
void sd_cd_init(void);

/* Initialise the SD card over SPI.  Must be called after InitializeTFT().
   Returns true on success, false if the card failed to initialise. */
bool sd_init(void);

/* Return true if a card is physically present in the socket.
   Reads the CD switch; does not talk to the card over SPI. */
bool sd_card_present(void);

/* Read one 512-byte block (sector) from the SD card.
   block : zero-based block address (byte address / 512).
   buf   : caller-supplied buffer, must be at least 512 bytes.
   Returns true on success.
   On failure, sd_last_r1 holds the CMD17 R1 byte, or 0xFF for token timeout. */
bool sd_read_block(uint32_t block, uint8_t *buf);
extern uint8_t sd_last_r1;

#endif /* SD_H */
