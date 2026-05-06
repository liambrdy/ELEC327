#include "sd.h"
#include "delay.h"
#include <ti/devices/msp/msp.h>

static uint8_t spi_byte(uint8_t b) {
    while (!(SPI0->STAT & SPI_STAT_TNF_MASK)) {}
    SPI0->TXDATA = b;
    while (SPI0->STAT & SPI_STAT_RFE_MASK) {}
    return (uint8_t)(SPI0->RXDATA & 0xFF);
}

static void sd_cs_low(void)  { SD_CS_PORT->DOUTCLR31_0 = SD_CS_PIN; }
static void sd_cs_high(void) { SD_CS_PORT->DOUTSET31_0 = SD_CS_PIN; }

static void spi_set_clkctl(uint32_t clkctl) {
    while (SPI0->STAT & SPI_STAT_BUSY_MASK) {}
    SPI0->CTL1 &= ~SPI_CTL1_ENABLE_MASK;
    SPI0->CLKCTL = clkctl;
    SPI0->CTL1 |= SPI_CTL1_ENABLE_ENABLE;
}

/* Poll up to 64 bytes for a non-0xFF response. */
static uint8_t sd_wait_r1(void) {
    for (int i = 0; i < 64; i++) {
        uint8_t r = spi_byte(0xFF);
        if (r != 0xFF) return r;
    }
    return 0xFF;
}

static uint8_t sd_cmd(uint8_t cmd, uint32_t arg, uint8_t crc) {
    sd_cs_low();
    spi_byte(0xFF);
    spi_byte(0x40 | cmd);
    spi_byte((uint8_t)(arg >> 24));
    spi_byte((uint8_t)(arg >> 16));
    spi_byte((uint8_t)(arg >>  8));
    spi_byte((uint8_t)(arg      ));
    spi_byte(crc);
    return sd_wait_r1();
}

static uint8_t _hc;

bool sd_card_present(void) {
    uint32_t bit = SD_CD_PORT->DIN31_0 & SD_CD_PIN;
#if SD_CD_ACTIVE_LOW
    return (bit == 0);
#else
    return (bit != 0);
#endif
}

void sd_cd_init(void) {
    IOMUX->SECCFG.PINCM[SD_CD_PINCM] =
        IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM_INENA_ENABLE |
        IOMUX_PINCM_PIPU_ENABLE  | SD_CD_PF;
}

bool sd_init(void) {
    IOMUX->SECCFG.PINCM[SD_CS_PINCM] = IOMUX_PINCM_PC_CONNECTED | SD_CS_PF;
    SD_CS_PORT->DOESET31_0  = SD_CS_PIN;
    SD_CS_PORT->DOUTSET31_0 = SD_CS_PIN;

    /* Slow to ≤400 kHz for power-up: 32 MHz / (2*(1+39)) = 400 kHz. */
    spi_set_clkctl(39);
    delay_cycles(32000);

    sd_cs_high();
    for (int i = 0; i < 10; i++) spi_byte(0xFF);  /* ≥74 dummy clocks */

    uint8_t r = sd_cmd(0, 0, 0x95);  /* CMD0: expect 0x01 */
    sd_cs_high(); spi_byte(0xFF);
    if (r != 0x01) goto fail;

    /* CMD8: distinguish SDHC from SDv1. Arg: 2.7-3.6V range, check pattern 0xAA. */
    r = sd_cmd(8, 0x000001AA, 0x87);
    if (r == 0x01) {
        spi_byte(0xFF); spi_byte(0xFF); spi_byte(0xFF); spi_byte(0xFF);
        _hc = 1;
    } else {
        _hc = 0;
    }
    sd_cs_high(); spi_byte(0xFF);

    /* ACMD41: spin until card leaves idle. CMD55 must precede each ACMD. */
    for (int i = 0; i < 2000; i++) {
        r = sd_cmd(55, 0, 0x01);
        sd_cs_high(); spi_byte(0xFF);
        if (r > 0x01) goto fail;

        r = sd_cmd(41, _hc ? 0x40000000u : 0, 0x01);
        sd_cs_high(); spi_byte(0xFF);
        if (r == 0x00) break;
        delay_cycles(16000);
    }
    if (r != 0x00) goto fail;

    /* CMD58: read CCS bit to confirm block vs byte addressing. */
    r = sd_cmd(58, 0, 0x01);
    if (r == 0x00) {
        uint8_t ocr0 = spi_byte(0xFF);
        spi_byte(0xFF); spi_byte(0xFF); spi_byte(0xFF);
        _hc = (ocr0 & 0x40) ? 1 : 0;  /* CCS=1 → block addressing */
    }
    sd_cs_high(); spi_byte(0xFF);

    if (!_hc) {
        r = sd_cmd(16, 512, 0x01);  /* CMD16: set block length (SDv1 only) */
        sd_cs_high(); spi_byte(0xFF);
        if (r != 0x00) goto fail;
    }

    spi_set_clkctl(0);  /* restore 16 MHz */
    return true;

fail:
    sd_cs_high();
    spi_set_clkctl(0);
    return false;
}

uint8_t sd_last_r1;

bool sd_read_block(uint32_t block, uint8_t *buf) {
    uint32_t addr = _hc ? block : block * 512u;

    uint8_t r = sd_cmd(17, addr, 0x01);
    sd_last_r1 = r;
    if (r != 0x00) { sd_cs_high(); spi_byte(0xFF); return false; }

    /* Wait for data token 0xFE. At 16 MHz: 400000 iterations ≈ 200 ms. */
    uint8_t tok = 0xFF;
    for (int i = 0; i < 400000; i++) {
        tok = spi_byte(0xFF);
        if (tok != 0xFF) break;
    }
    if (tok != 0xFE) {
        sd_last_r1 = 0xFF;
        sd_cs_high(); spi_byte(0xFF); return false;
    }

    for (int i = 0; i < 512; i++) buf[i] = spi_byte(0xFF);
    spi_byte(0xFF); spi_byte(0xFF);  /* discard CRC */

    sd_cs_high();
    spi_byte(0xFF); spi_byte(0xFF); spi_byte(0xFF);
    return true;
}
