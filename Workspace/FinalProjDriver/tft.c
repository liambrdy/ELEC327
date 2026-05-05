
#include "tft.h"
#include "delay.h"
#include <ti/devices/msp/msp.h>
#include <ti/driverlib/dl_dma.h>
#include <ti/driverlib/dl_spi.h>

#include <stdlib.h>
#include <string.h>

volatile bool spi_wakeup;

#define DC_PIN  (1UL << 26)  /* PA26 */
#define RST_PIN (1UL << 27)  /* PA27 */
#define CS_PIN  (1UL << 3)   /* PA3  */

#define DMA_CH_SPI0_TX  0u
#define STRIP_BYTES     (4u * 320u * 2u)  /* 4 rows × 320px × 2 bytes */

static uint8_t  strip[2][STRIP_BYTES];
static uint8_t  active;
static uint32_t fill_pos;

static volatile bool dma_busy;
static bool          cs_held;  /* deassert CS on next TFTWaitIdle */

static void DCHigh(void) { GPIOA->DOUTSET31_0 = DC_PIN; }
static void DCLow(void)  { GPIOA->DOUTCLR31_0 = DC_PIN; }
static void CSLow(void)  { GPIOA->DOUTCLR31_0 = CS_PIN; }
static void CSHigh(void) { GPIOA->DOUTSET31_0 = CS_PIN; }

static void spi_txrx(uint8_t byte) {
    while (!(SPI0->STAT & SPI_STAT_TNF_MASK)) {}
    SPI0->TXDATA = byte;
    while (SPI0->STAT & SPI_STAT_RFE_MASK) {}
    volatile uint32_t dummy = SPI0->RXDATA;
}

static void spi_drain_rx(void) {
    while (!(SPI0->STAT & SPI_STAT_RFE_MASK)) {
        volatile uint32_t _ = SPI0->RXDATA;
    }
}

static void spi_flush(void) {
    while (SPI0->STAT & SPI_STAT_BUSY_MASK) {}
    spi_drain_rx();
}

static void SPISendBlocking(const uint8_t *tx, uint32_t len) {
    for (uint32_t i = 0; i < len; i++)
        spi_txrx(tx[i]);
}

void SPI0_IRQHandler(void) {
    switch (DL_SPI_getPendingInterrupt(SPI0)) {
        case DL_SPI_IIDX_DMA_DONE_TX:
            break;  /* wait for TX_EMPTY before signalling done */
        case DL_SPI_IIDX_TX_EMPTY:
            dma_busy   = false;
            spi_wakeup = true;
            break;
        default:
            break;
    }
}

void TFTWaitIdle(void) {
    while (dma_busy) {}
    spi_drain_rx();
    while (SPI0->STAT & SPI_STAT_BUSY_MASK) {}
    if (cs_held) {
        CSHigh();
        cs_held = false;
    }
}

static void tft_flush_strip(void) {
    uint32_t n = fill_pos;
    uint8_t to_send = active;
    active ^= 1u;
    fill_pos = 0;
    while (dma_busy) {}
    while (SPI0->STAT & SPI_STAT_BUSY_MASK) {}
    if (n == 0) return;
    dma_busy = true;
    DL_DMA_setSrcAddr(DMA, DMA_CH_SPI0_TX, (uint32_t)strip[to_send]);
    DL_DMA_setTransferSize(DMA, DMA_CH_SPI0_TX, n);
    DL_DMA_enableChannel(DMA, DMA_CH_SPI0_TX);
}

void InitializeTFT(void) {
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM11)] = IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM11_PF_SPI0_SCLK;  /* PA6 */
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM9)]  = IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM_INENA_ENABLE | IOMUX_PINCM9_PF_SPI0_POCI;  /* PA4 */
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM10)] = IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM10_PF_SPI0_PICO;  /* PA5 */

    IOMUX->SECCFG.PINCM[(IOMUX_PINCM8)]  = IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM8_PF_GPIOA_DIO03;
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM59)] = IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM59_PF_GPIOA_DIO26;
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM60)] = IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM60_PF_GPIOA_DIO27;

    /* Set HIGH before enabling output to avoid a glitch on CS/RST. */
    GPIOA->DOUTSET31_0 = CS_PIN | DC_PIN | RST_PIN;
    GPIOA->DOESET31_0  = CS_PIN | DC_PIN | RST_PIN;

    SPI0->GPRCM.RSTCTL = (SPI_RSTCTL_KEY_UNLOCK_W | SPI_RSTCTL_RESETSTKYCLR_CLR | SPI_RSTCTL_RESETASSERT_ASSERT);
    SPI0->GPRCM.PWREN  = (SPI_PWREN_KEY_UNLOCK_W | SPI_PWREN_ENABLE_ENABLE);
    delay_cycles(POWER_STARTUP_DELAY);

    SPI0->CLKSEL = (uint32_t)SPI_CLKSEL_SYSCLK_SEL_ENABLE;
    SPI0->CLKDIV = (uint32_t)SPI_CLKDIV_RATIO_DIV_BY_1;

    SPI0->CTL0 = SPI_CTL0_SPO_LOW           |
                 SPI_CTL0_SPH_FIRST          |
                 SPI_CTL0_PACKEN_DISABLED    |
                 SPI_CTL0_CSCLR_DISABLE      |
                 SPI_CTL0_FRF_MOTOROLA_4WIRE |
                 SPI_CTL0_DSS_DSS_8;

    SPI0->CTL1 = SPI_CTL1_CP_ENABLE   |
                 SPI_CTL1_PREN_DISABLE |
                 SPI_CTL1_PTEN_DISABLE |
                 SPI_CTL1_PES_DISABLE  |
                 SPI_CTL1_MSB_ENABLE;

    SPI0->CLKCTL = 0;  /* SCR=0 → 16 MHz */
    SPI0->CTL1 |= SPI_CTL1_ENABLE_ENABLE;

    GPIOA->DOUTSET31_0 = RST_PIN;
    delay_cycles(10 * 32000);
    GPIOA->DOUTCLR31_0 = RST_PIN;
    delay_cycles(20 * 32000);
    GPIOA->DOUTSET31_0 = RST_PIN;
    delay_cycles(150 * 32000);
}

void TFTInitDMA(void) {
    static const DL_DMA_Config cfg = {
        .transferMode  = DL_DMA_SINGLE_TRANSFER_MODE,
        .extendedMode  = DL_DMA_NORMAL_MODE,
        .destIncrement = DL_DMA_ADDR_UNCHANGED,
        .srcIncrement  = DL_DMA_ADDR_INCREMENT,
        .destWidth     = DL_DMA_WIDTH_BYTE,
        .srcWidth      = DL_DMA_WIDTH_BYTE,
        .trigger       = DMA_SPI0_TX_TRIG,
        .triggerType   = DL_DMA_TRIGGER_TYPE_EXTERNAL,
    };
    DL_DMA_initChannel(DMA, DMA_CH_SPI0_TX, (DL_DMA_Config *)&cfg);
    DL_DMA_setDestAddr(DMA, DMA_CH_SPI0_TX, (uint32_t)(&SPI0->TXDATA));

    DL_SPI_enableDMATransmitEvent(SPI0);
    DL_SPI_setFIFOThreshold(SPI0,
        DL_SPI_RX_FIFO_LEVEL_ONE_FRAME,
        DL_SPI_TX_FIFO_LEVEL_ONE_FRAME);
    DL_SPI_enableInterrupt(SPI0,
        DL_SPI_INTERRUPT_DMA_DONE_TX | DL_SPI_INTERRUPT_TX_EMPTY);
    NVIC_EnableIRQ(SPI0_INT_IRQn);

    active   = 0;
    fill_pos = 0;
    dma_busy = false;
    cs_held  = false;
}

void TFTWriteData(uint8_t *data, uint16_t len) {
    if (!data || len == 0) return;
    TFTWaitIdle();
    CSLow(); DCHigh();
    SPISendBlocking(data, len);
    spi_flush();
    CSHigh();
}

void TFTWriteCommand(uint8_t cmd, uint8_t *params, uint16_t len) {
    TFTWaitIdle();
    CSLow();
    DCLow();
    SPISendBlocking(&cmd, 1);
    if (params && len > 0) {
        DCHigh();
        SPISendBlocking(params, len);
    }
    spi_flush();
    CSHigh();
    DCHigh();
}

void TFTReadCommand(uint8_t cmd, uint8_t *out, uint16_t len) {
    if (!out || len == 0 || len > 64) return;
    TFTWaitIdle();
    CSLow();
    DCLow();
    spi_txrx(cmd);
    DCHigh();
    for (uint16_t i = 0; i < len; i++) {
        while (!(SPI0->STAT & SPI_STAT_TNF_MASK)) {}
        SPI0->TXDATA = 0x00;
        while (SPI0->STAT & SPI_STAT_RFE_MASK) {}
        out[i] = (uint8_t)(SPI0->RXDATA & 0xFF);
    }
    CSHigh();
}

void TFTFillRegion(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
    TFTWaitIdle();

    uint8_t p[4];
    CSLow();

    DCLow(); spi_txrx(0x2A);
    p[0]=x0>>8; p[1]=x0&0xFF; p[2]=x1>>8; p[3]=x1&0xFF;
    DCHigh(); SPISendBlocking(p, 4);

    DCLow(); spi_txrx(0x2B);
    p[0]=y0>>8; p[1]=y0&0xFF; p[2]=y1>>8; p[3]=y1&0xFF;
    DCHigh(); SPISendBlocking(p, 4);

    DCLow(); spi_txrx(0x2C); DCHigh();

    uint8_t  hi    = color >> 8;
    uint8_t  lo    = color & 0xFF;
    uint32_t total = 2u * (uint32_t)(x1 - x0 + 1u) * (y1 - y0 + 1u);
    uint32_t sent  = 0;

    active   = 0;
    fill_pos = 0;

    while (sent < total) {
        uint32_t chunk = total - sent;
        if (chunk > STRIP_BYTES) chunk = STRIP_BYTES;
        uint8_t *buf = strip[active];
        for (uint32_t i = 0; i < chunk; i += 2) {
            buf[i]   = hi;
            buf[i+1] = lo;
        }
        fill_pos = chunk;
        sent    += chunk;
        tft_flush_strip();
    }

    cs_held = true;
}

void TFTBeginPixels(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    TFTWaitIdle();

    uint8_t p[4];
    p[0]=x0>>8; p[1]=x0&0xFF; p[2]=x1>>8; p[3]=x1&0xFF;
    TFTWriteCommand(0x2A, p, 4);
    p[0]=y0>>8; p[1]=y0&0xFF; p[2]=y1>>8; p[3]=y1&0xFF;
    TFTWriteCommand(0x2B, p, 4);

    CSLow(); DCLow(); spi_txrx(0x2C); DCHigh();

    active   = 0;
    fill_pos = 0;
}

void TFTSendPixel(uint16_t color) {
    strip[active][fill_pos++] = color >> 8;
    strip[active][fill_pos++] = color & 0xFF;
    if (fill_pos >= STRIP_BYTES)
        tft_flush_strip();
}

void TFTEndPixels(void) {
    tft_flush_strip();
    cs_held = true;
}

void TFTScrollDefine(uint16_t left_fixed, uint16_t scroll_width, uint16_t right_fixed) {
    uint8_t p[6];
    p[0]=left_fixed>>8;   p[1]=left_fixed&0xFF;
    p[2]=scroll_width>>8; p[3]=scroll_width&0xFF;
    p[4]=right_fixed>>8;  p[5]=right_fixed&0xFF;
    TFTWriteCommand(0x33, p, 6);
}

void TFTScrollSet(uint16_t pos) {
    uint8_t p[2];
    p[0]=pos>>8; p[1]=pos&0xFF;
    TFTWriteCommand(0x37, p, 2);
}

void SPISetCDMode(uint8_t mode) {
    SPI0->CTL1 &= ~SPI_CTL1_CDMODE_MASK;
    SPI0->CTL1 |= (mode << SPI_CTL1_CDMODE_OFS);
}

void SPIWaitDone(void) {
    TFTWaitIdle();
}

bool SPITransferAsync(uint8_t *tx, uint8_t *rx, uint32_t len) {
    if (len == 0) return false;
    TFTWaitIdle();
    spi_wakeup = false;
    for (uint32_t i = 0; i < len; i++) {
        while (!(SPI0->STAT & SPI_STAT_TNF_MASK)) {}
        SPI0->TXDATA = tx ? tx[i] : 0x00;
        while (SPI0->STAT & SPI_STAT_RFE_MASK) {}
        uint8_t r = (uint8_t)(SPI0->RXDATA & 0xFF);
        if (rx) rx[i] = r;
    }
    spi_wakeup = true;
    return true;
}
