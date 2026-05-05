#include "sd.h"
#include "delay.h"
#include <ti/devices/msp/msp.h>

/*
 * SD card SPI driver.
 *
 * Shares SPI1 with the TFT display.  The TFT CS and SD CS are separate GPIO
 * pins so the two devices never conflict — only one CS is ever asserted at a
 * time.
 *
 * SPI mode: CPOL=0 CPHA=0 (Mode 0) — same as the TFT, so no reconfiguration
 * between devices.  The clock is slowed to ≤400 kHz during the SD power-up
 * sequence and restored to 16 MHz afterwards.
 */

/* ---- SPI helpers (SPI1, same as TFT) ------------------------------------ */

static uint8_t spi_byte(uint8_t b) {
    while (!(SPI1->STAT & SPI_STAT_TNF_MASK)) {}
    SPI1->TXDATA = b;
    while (SPI1->STAT & SPI_STAT_RFE_MASK) {}
    return (uint8_t)(SPI1->RXDATA & 0xFF);
}

/* ---- CS helpers --------------------------------------------------------- */

static void sd_cs_low(void)  { SD_CS_PORT->DOUTCLR31_0 = SD_CS_PIN; }
static void sd_cs_high(void) { SD_CS_PORT->DOUTSET31_0 = SD_CS_PIN; }

/* ---- Clock speed helpers ----------------------------------------------- */

static void spi_set_clkctl(uint32_t clkctl) {
    while (SPI1->STAT & SPI_STAT_BUSY_MASK) {}  /* wait for idle */
    SPI1->CTL1 &= ~SPI_CTL1_ENABLE_MASK;
    SPI1->CLKCTL = clkctl;
    SPI1->CTL1 |= SPI_CTL1_ENABLE_ENABLE;
}

/* ---- SD command helpers ------------------------------------------------- */

/* Poll up to 64 bytes for a non-0xFF response. */
static uint8_t sd_wait_r1(void) {
    for (int i = 0; i < 64; i++) {
        uint8_t r = spi_byte(0xFF);
        if (r != 0xFF) return r;
    }
    return 0xFF; /* timeout */
}

/*
 * Send a standard 6-byte SD SPI command:
 *   [0x40|cmd] [arg[31:24]] [arg[23:16]] [arg[15:8]] [arg[7:0]] [crc]
 *
 * CS must be high before calling; function asserts CS and leaves it asserted
 * so that the caller can read additional response bytes.
 */
static uint8_t sd_cmd(uint8_t cmd, uint32_t arg, uint8_t crc) {
    sd_cs_low();
    spi_byte(0xFF);                         /* 8 dummy clocks before command */
    spi_byte(0x40 | cmd);
    spi_byte((uint8_t)(arg >> 24));
    spi_byte((uint8_t)(arg >> 16));
    spi_byte((uint8_t)(arg >>  8));
    spi_byte((uint8_t)(arg      ));
    spi_byte(crc);
    return sd_wait_r1();
}

/* ---- Card type flag ----------------------------------------------------- */

static uint8_t _hc; /* 1 = SDHC/SDXC (block addressing), 0 = SDv1 (byte addressing) */

/* ---- Public API --------------------------------------------------------- */

bool sd_card_present(void) {
    uint32_t bit = SD_CD_PORT->DIN31_0 & SD_CD_PIN;
#if SD_CD_ACTIVE_LOW
    return (bit == 0);   /* LOW  = card inserted */
#else
    return (bit != 0);   /* HIGH = card inserted */
#endif
}

void sd_cd_init(void) {
    IOMUX->SECCFG.PINCM[SD_CD_PINCM] =
        IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM_INENA_ENABLE |
        IOMUX_PINCM_PIPU_ENABLE  | SD_CD_PF;
}

bool sd_init(void) {
    /* Configure SD CS pin as GPIO output, start deasserted (high). */
    IOMUX->SECCFG.PINCM[SD_CS_PINCM] = IOMUX_PINCM_PC_CONNECTED | SD_CS_PF;
    SD_CS_PORT->DOESET31_0  = SD_CS_PIN;
    SD_CS_PORT->DOUTSET31_0 = SD_CS_PIN;

    /* Slow SPI to ≤400 kHz for SD power-up sequence.
       At 32 MHz: f = 32 MHz / (2*(1+SCR))  →  SCR=39 gives 400 kHz. */
    spi_set_clkctl(39);

    delay_cycles(32000); /* ≥1 ms after VCC stable */

    /* Send ≥74 dummy clocks with CS deasserted to put card in SPI mode. */
    sd_cs_high();
    for (int i = 0; i < 10; i++) spi_byte(0xFF);

    /* CMD0: GO_IDLE_STATE — expect R1 = 0x01 (in idle). */
    uint8_t r = sd_cmd(0, 0, 0x95);
    sd_cs_high();
    spi_byte(0xFF);
    if (r != 0x01) goto fail;

    /* CMD8: SEND_IF_COND — distinguishes SDHC from older SDv1 cards.
       Argument 0x000001AA: voltage 2.7-3.6V, check pattern 0xAA. */
    r = sd_cmd(8, 0x000001AA, 0x87);
    if (r == 0x01) {
        /* SDHC/SDXC: read the 4-byte R7 trailer. */
        spi_byte(0xFF); spi_byte(0xFF); spi_byte(0xFF); spi_byte(0xFF);
        _hc = 1;
    } else {
        _hc = 0;
    }
    sd_cs_high();
    spi_byte(0xFF);

    /* ACMD41: APP_SEND_OP_COND — spin until card leaves idle state.
       Must be preceded by CMD55 (APP_CMD) each iteration. */
    for (int i = 0; i < 2000; i++) {
        /* CMD55 */
        r = sd_cmd(55, 0, 0x01);
        sd_cs_high();
        spi_byte(0xFF);
        if (r > 0x01) goto fail;

        /* CMD41 with HCS=1 if SDHC */
        uint32_t acmd41_arg = _hc ? 0x40000000u : 0x00000000u;
        r = sd_cmd(41, acmd41_arg, 0x01);
        sd_cs_high();
        spi_byte(0xFF);
        if (r == 0x00) break;
        delay_cycles(16000); /* ~0.5 ms between retries (2000 × 0.5 ms = 1 s total per SD spec) */
    }
    if (r != 0x00) goto fail;

    /* CMD58: READ_OCR — read CCS bit to confirm block vs byte addressing. */
    r = sd_cmd(58, 0, 0x01);
    if (r == 0x00) {
        uint8_t ocr0 = spi_byte(0xFF);
        spi_byte(0xFF); spi_byte(0xFF); spi_byte(0xFF);
        _hc = (ocr0 & 0x40) ? 1 : 0; /* CCS=1 → SDHC/SDXC block-addr; CCS=0 → SDSC byte-addr */
    }
    sd_cs_high();
    spi_byte(0xFF);

    /* CMD16: SET_BLOCKLEN to 512 bytes (only required for SDv1). */
    if (!_hc) {
        r = sd_cmd(16, 512, 0x01);
        sd_cs_high();
        spi_byte(0xFF);
        if (r != 0x00) goto fail;
    }

    /* Restore 16 MHz SPI clock for normal operation. */
    spi_set_clkctl(0);
    return true;

fail:
    sd_cs_high();
    spi_set_clkctl(0);
    return false;
}

uint8_t sd_last_r1;   /* R1 from last CMD17; 0xFF = data-token timeout */

bool sd_read_block(uint32_t block, uint8_t *buf) {
    /* SDHC uses block addresses; SDv1 uses byte addresses. */
    uint32_t addr = _hc ? block : block * 512u;

    uint8_t r = sd_cmd(17, addr, 0x01); /* CMD17: READ_SINGLE_BLOCK */
    sd_last_r1 = r;
    if (r != 0x00) { sd_cs_high(); spi_byte(0xFF); return false; }

    /* Wait for data start token 0xFE.  SD spec allows up to ~200 ms;
       at 16 MHz each spi_byte = 500 ns → 400000 iterations ≈ 200 ms. */
    uint8_t tok = 0xFF;
    for (int i = 0; i < 400000; i++) {
        tok = spi_byte(0xFF);
        if (tok != 0xFF) break;
    }
    if (tok != 0xFE) {
        sd_last_r1 = 0xFF; /* sentinel: token timeout */
        sd_cs_high(); spi_byte(0xFF); return false;
    }

    /* Read 512 data bytes then 2 CRC bytes (discarded). */
    for (int i = 0; i < 512; i++) buf[i] = spi_byte(0xFF);
    spi_byte(0xFF); /* CRC high */
    spi_byte(0xFF); /* CRC low  */

    sd_cs_high();
    spi_byte(0xFF); spi_byte(0xFF); spi_byte(0xFF); /* 24 clock recovery after read */
    return true;
}
