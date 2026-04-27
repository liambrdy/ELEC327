
#include "tft.h"
#include "delay.h"
#include <ti/devices/msp/msp.h>

#include <stdlib.h>
#include <string.h>

// PUBLIC
volatile bool spi_wakeup;

// PRIVATE
static const uint8_t *spi_tx_buf;
static uint8_t *spi_rx_buf;
static volatile uint32_t spi_len;
static volatile uint32_t spi_tx_idx;   // bytes fed into TX FIFO so far
static volatile uint32_t spi_rx_count; // bytes received from RX FIFO so far
static volatile bool spi_busy;

#define DC_PIN  (1UL << 1)   // PB1  = D/C
#define RST_PIN (1UL << 16)  // PB16 = RST
#define CS_PIN  (1UL << 6)   // PB6  = CS (PINCM23, driven manually as GPIO)

void InitializeTFT(void) {
    // SPI data/clock pins
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM26)] = IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM26_PF_SPI1_SCLK;
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM24)] = IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM24_PF_SPI1_POCI;
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM25)] = IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM25_PF_SPI1_PICO;

    // CS, DC, RST as manual GPIO outputs — CS and DC start high, RST starts high
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM23)] = IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM23_PF_GPIOB_DIO06;
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM13)] = IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM13_PF_GPIOB_DIO01;
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM33)] = IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM33_PF_GPIOB_DIO16;

    GPIOB->DOESET31_0  = CS_PIN | DC_PIN | RST_PIN;
    GPIOB->DOUTSET31_0 = CS_PIN | DC_PIN | RST_PIN; // CS=high (deasserted), DC=high, RST=high

    SPI1->GPRCM.RSTCTL = (SPI_RSTCTL_KEY_UNLOCK_W | SPI_RSTCTL_RESETSTKYCLR_CLR | SPI_RSTCTL_RESETASSERT_ASSERT);
    SPI1->GPRCM.PWREN = (SPI_PWREN_KEY_UNLOCK_W | SPI_PWREN_ENABLE_ENABLE);
    delay_cycles(POWER_STARTUP_DELAY);

    SPI1->CLKSEL = (uint32_t) SPI_CLKSEL_SYSCLK_SEL_ENABLE;
    SPI1->CLKDIV = (uint32_t) SPI_CLKDIV_RATIO_DIV_BY_1;

    SPI1->CTL0 = SPI_CTL0_SPO_LOW          |
                 SPI_CTL0_SPH_FIRST         |
                 SPI_CTL0_PACKEN_DISABLED   |
                 SPI_CTL0_CSCLR_DISABLE     | // CS is manual GPIO; don't let hardware touch it
                 SPI_CTL0_FRF_MOTOROLA_4WIRE |
                 SPI_CTL0_DSS_DSS_8;

    SPI1->CTL1 = SPI_CTL1_CP_ENABLE     |
                 SPI_CTL1_PREN_DISABLE   |
                 SPI_CTL1_PTEN_DISABLE   |
                 SPI_CTL1_PES_DISABLE    |
                 SPI_CTL1_MSB_ENABLE;

    /*
     * outputBitRate = (spiInputClock) / ((1 + SCR) * 2)
     * 2000000 = (32000000) / ((1 + 7) * 2)
     */
    SPI1->CLKCTL = 0; // 8 MHz — change to 0 for 16 MHz if signal is clean

    SPI1->CTL1 |= SPI_CTL1_ENABLE_ENABLE;

    // Hardware reset sequence
    GPIOB->DOUTSET31_0 = RST_PIN;
    delay_cycles(10 * 32000);
    GPIOB->DOUTCLR31_0 = RST_PIN;
    delay_cycles(20 * 32000);
    GPIOB->DOUTSET31_0 = RST_PIN;
    delay_cycles(150 * 32000);
}

static void DCHigh(void) { GPIOB->DOUTSET31_0 = DC_PIN; }
static void DCLow(void)  { GPIOB->DOUTCLR31_0 = DC_PIN; }
static void CSLow(void)  { GPIOB->DOUTCLR31_0 = CS_PIN; }
static void CSHigh(void) { GPIOB->DOUTSET31_0 = CS_PIN; }

static void spi_txrx(uint8_t byte) {
    while (!(SPI1->STAT & SPI_STAT_TNF_MASK)) {}   // wait for TX FIFO space
    SPI1->TXDATA = byte;
    while (SPI1->STAT & SPI_STAT_RFE_MASK) {}       // wait until RX byte arrives
    volatile uint32_t dummy = SPI1->RXDATA;
}

// Push one byte into TX FIFO without draining RX — caller must drain before CSHigh.
static inline void spi_tx(uint8_t byte) {
    while (!(SPI1->STAT & SPI_STAT_TNF_MASK)) {}
    SPI1->TXDATA = byte;
}

// Drain every pending byte from the RX FIFO (discarding all).
static void spi_drain_rx(void) {
    while (!(SPI1->STAT & SPI_STAT_RFE_MASK)) {
        volatile uint32_t _ = SPI1->RXDATA;
    }
}

// Wait for SPI to finish all in-flight bits, then drain RX.
static void spi_flush(void) {
    while (SPI1->STAT & SPI_STAT_BUSY_MASK) {}
    spi_drain_rx();
}

static void SPISendBlocking(const uint8_t *tx, uint32_t len) {
    if (!tx || len == 0) return;
    for (uint32_t i = 0; i < len; i++) {
        spi_txrx(tx[i]);
    }
}

void TFTWriteData(uint8_t *data, uint16_t len) {
    if (!data || len == 0) return;
    CSLow();
    DCHigh();
    SPISendBlocking(data, len);
    CSHigh();
}

void TFTWriteCommand(uint8_t cmd, uint8_t *params, uint16_t len) {
    CSLow();

    DCLow();
    SPISendBlocking(&cmd, 1);

    if (params && len > 0) {
        DCHigh();
        SPISendBlocking(params, len);
    }

    CSHigh();
    DCHigh();
}

void TFTReadCommand(uint8_t cmd, uint8_t *out, uint16_t len) {
    if (!out || len == 0 || len > 64) return;

    CSLow();
    DCLow();
    spi_txrx(cmd); // discard dummy response byte

    DCHigh();
    for (uint16_t i = 0; i < len; i++) {
        while (!(SPI1->STAT & SPI_STAT_TNF_MASK)) {}
        SPI1->TXDATA = 0x00;
        while (SPI1->STAT & SPI_STAT_RFE_MASK) {}
        out[i] = (uint8_t)(SPI1->RXDATA & 0xFF);
    }

    CSHigh();
}

// CASET + PASET + RAMWR + pixels — all under one CS assertion.
void TFTFillRegion(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
    uint8_t cmd, p[4];

    CSLow();

    // CASET
    cmd = 0x2A;
    DCLow(); spi_txrx(cmd);
    p[0] = x0 >> 8; p[1] = x0 & 0xFF; p[2] = x1 >> 8; p[3] = x1 & 0xFF;
    DCHigh(); SPISendBlocking(p, 4);

    // PASET
    cmd = 0x2B;
    DCLow(); spi_txrx(cmd);
    p[0] = y0 >> 8; p[1] = y0 & 0xFF; p[2] = y1 >> 8; p[3] = y1 & 0xFF;
    DCHigh(); SPISendBlocking(p, 4);

    // RAMWR then pixel data
    DCLow(); spi_txrx(0x2C);
    DCHigh();

    uint8_t hi = color >> 8, lo = color & 0xFF;
    uint32_t count = (uint32_t)(x1 - x0 + 1) * (y1 - y0 + 1);

    // Pipeline TX; drain RX in bursts to prevent FIFO overflow.
    uint32_t rx_pending = 0;
    for (uint32_t i = 0; i < count; i++) {
        spi_tx(hi); rx_pending++;
        spi_tx(lo); rx_pending++;
        if (rx_pending >= 4) {
            spi_drain_rx();
            rx_pending = 0;
        }
    }
    spi_flush();
    CSHigh();
}

// Streaming pixel API: set window, stream pixels one at a time, then end.
// Keeps CS low across the entire operation — no per-row CS toggling.
void TFTBeginPixels(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t p[4];
    p[0] = x0 >> 8; p[1] = x0 & 0xFF; p[2] = x1 >> 8; p[3] = x1 & 0xFF;
    TFTWriteCommand(0x2A, p, 4);
    p[0] = y0 >> 8; p[1] = y0 & 0xFF; p[2] = y1 >> 8; p[3] = y1 & 0xFF;
    TFTWriteCommand(0x2B, p, 4);
    CSLow();
    DCLow();
    spi_txrx(0x2C);
    DCHigh();
    // CS stays low — caller streams pixels via TFTSendPixel, then calls TFTEndPixels
}

void TFTSendPixel(uint16_t color) {
    spi_txrx(color >> 8);
    spi_txrx(color & 0xFF);
}

void TFTEndPixels(void) {
    CSHigh();
}

/* ---- Hardware scroll ---------------------------------------------------- */

void TFTScrollDefine(uint16_t left_fixed, uint16_t scroll_width, uint16_t right_fixed) {
    uint8_t p[6];
    p[0] = left_fixed    >> 8; p[1] = left_fixed    & 0xFF;
    p[2] = scroll_width  >> 8; p[3] = scroll_width  & 0xFF;
    p[4] = right_fixed   >> 8; p[5] = right_fixed   & 0xFF;
    TFTWriteCommand(0x33, p, 6);
}

void TFTScrollSet(uint16_t pos) {
    uint8_t p[2];
    p[0] = pos >> 8;
    p[1] = pos & 0xFF;
    TFTWriteCommand(0x37, p, 2);
}

void SPISetCDMode(uint8_t mode) {
    SPI1->CTL1 &= ~SPI_CTL1_CDMODE_MASK;
    SPI1->CTL1 |= (mode << SPI_CTL1_CDMODE_OFS);
}

void SPIWaitDone(void) {
    while (SPI1->STAT & SPI_STAT_BUSY_MASK) {}
}

bool SPITransferAsync(uint8_t *tx, uint8_t *rx, uint32_t len) {
    if (len == 0) return false;
    spi_wakeup = false;
    for (uint32_t i = 0; i < len; i++) {
        while (!(SPI1->STAT & SPI_STAT_TNF_MASK)) {}
        SPI1->TXDATA = tx ? tx[i] : 0x00;
        while (SPI1->STAT & SPI_STAT_RFE_MASK) {}
        uint8_t r = (uint8_t)(SPI1->RXDATA & 0xFF);
        if (rx) rx[i] = r;
    }
    spi_wakeup = true;
    return true;
}
