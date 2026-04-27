
#include "tft.h"
#include "delay.h"
#include <ti/devices/msp/msp.h>
#include <ti/driverlib/dl_dma.h>
#include <ti/driverlib/dl_spi.h>

#include <stdlib.h>
#include <string.h>

/* ---- Public ---- */
volatile bool spi_wakeup;

/* ---- Pin definitions ---- */
#define DC_PIN  (1UL << 1)   /* PB1  = D/C */
#define RST_PIN (1UL << 16)  /* PB16 = RST */
#define CS_PIN  (1UL << 6)   /* PB6  = CS  */

/* ---- DMA strip-buffer state -------------------------------------------- *
 *
 * Pixel data is accumulated in one of two 2560-byte strips (4 rows × 320 px ×
 * 2 bytes).  When a strip is full (or the pixel burst ends), it is handed to
 * DMA channel 0 → SPI1 TXDATA.  While DMA sends the previous strip the CPU
 * fills the next one — double-buffered overlap.
 *
 * After the LAST strip is kicked, the drawing function returns immediately
 * (cs_held=true, CS still low).  The next drawing call or display_commit()
 * will call TFTWaitIdle, which blocks until TX_EMPTY, drains RX, and raises CS.
 */
#define DMA_CH_SPI1_TX   0u
#define STRIP_BYTES      (4u * 320u * 2u)   /* 2560 bytes */

static uint8_t  strip[2][STRIP_BYTES];
static uint8_t  active;      /* which strip the CPU is filling */
static uint32_t fill_pos;    /* write cursor within strip[active] */

static volatile bool dma_busy; /* a DMA transfer is in progress */
static bool          cs_held;  /* CS is asserted low; raise on TFTWaitIdle */

/* ---- SPI helpers ---- */

static void DCHigh(void) { GPIOB->DOUTSET31_0 = DC_PIN; }
static void DCLow(void)  { GPIOB->DOUTCLR31_0 = DC_PIN; }
static void CSLow(void)  { GPIOB->DOUTCLR31_0 = CS_PIN; }
static void CSHigh(void) { GPIOB->DOUTSET31_0 = CS_PIN; }

/* Synchronous byte TX+RX — used for commands and short data. */
static void spi_txrx(uint8_t byte) {
    while (!(SPI1->STAT & SPI_STAT_TNF_MASK)) {}
    SPI1->TXDATA = byte;
    while (SPI1->STAT & SPI_STAT_RFE_MASK) {}
    volatile uint32_t dummy = SPI1->RXDATA;
}

static void spi_drain_rx(void) {
    while (!(SPI1->STAT & SPI_STAT_RFE_MASK)) {
        volatile uint32_t _ = SPI1->RXDATA;
    }
}

static void spi_flush(void) {
    while (SPI1->STAT & SPI_STAT_BUSY_MASK) {}
    spi_drain_rx();
}

static void SPISendBlocking(const uint8_t *tx, uint32_t len) {
    for (uint32_t i = 0; i < len; i++)
        spi_txrx(tx[i]);
}

/* ---- SPI1 DMA ISR ---- */

void SPI1_IRQHandler(void) {
    switch (DL_SPI_getPendingInterrupt(SPI1)) {
        case DL_SPI_IIDX_DMA_DONE_TX:
            /* DMA has loaded all bytes into the TX FIFO.
               Wait for TX_EMPTY before clearing dma_busy. */
            break;
        case DL_SPI_IIDX_TX_EMPTY:
            /* TX FIFO is empty (shift register may still be running;
               TFTWaitIdle polls BUSY for that). */
            dma_busy  = false;
            spi_wakeup = true;
            break;
        default:
            break;
    }
}

/* ---- TFTWaitIdle ---- */

void TFTWaitIdle(void) {
    while (dma_busy) {}
    spi_drain_rx();
    while (SPI1->STAT & SPI_STAT_BUSY_MASK) {}
    if (cs_held) {
        CSHigh();
        cs_held = false;
    }
}

/* ---- Internal: flush active strip, swap, wait for prev DMA, kick new ---- */

static void tft_flush_strip(void) {
    uint32_t n       = fill_pos;
    uint8_t  to_send = active;
    active   ^= 1u;
    fill_pos  = 0;
    while (dma_busy) {}          /* wait for the previous DMA to finish */
    if (n == 0) return;
    dma_busy = true;
    DL_DMA_setSrcAddr(DMA, DMA_CH_SPI1_TX, (uint32_t)strip[to_send]);
    DL_DMA_setTransferSize(DMA, DMA_CH_SPI1_TX, n);
    DL_DMA_enableChannel(DMA, DMA_CH_SPI1_TX);
}

/* ---- InitializeTFT ---- */

void InitializeTFT(void) {
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM26)] = IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM26_PF_SPI1_SCLK;
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM24)] = IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM24_PF_SPI1_POCI;
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM25)] = IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM25_PF_SPI1_PICO;

    IOMUX->SECCFG.PINCM[(IOMUX_PINCM23)] = IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM23_PF_GPIOB_DIO06;
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM13)] = IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM13_PF_GPIOB_DIO01;
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM33)] = IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM33_PF_GPIOB_DIO16;

    GPIOB->DOESET31_0  = CS_PIN | DC_PIN | RST_PIN;
    GPIOB->DOUTSET31_0 = CS_PIN | DC_PIN | RST_PIN;

    SPI1->GPRCM.RSTCTL = (SPI_RSTCTL_KEY_UNLOCK_W | SPI_RSTCTL_RESETSTKYCLR_CLR | SPI_RSTCTL_RESETASSERT_ASSERT);
    SPI1->GPRCM.PWREN  = (SPI_PWREN_KEY_UNLOCK_W | SPI_PWREN_ENABLE_ENABLE);
    delay_cycles(POWER_STARTUP_DELAY);

    SPI1->CLKSEL = (uint32_t)SPI_CLKSEL_SYSCLK_SEL_ENABLE;
    SPI1->CLKDIV = (uint32_t)SPI_CLKDIV_RATIO_DIV_BY_1;

    SPI1->CTL0 = SPI_CTL0_SPO_LOW         |
                 SPI_CTL0_SPH_FIRST        |
                 SPI_CTL0_PACKEN_DISABLED  |
                 SPI_CTL0_CSCLR_DISABLE    |
                 SPI_CTL0_FRF_MOTOROLA_4WIRE |
                 SPI_CTL0_DSS_DSS_8;

    SPI1->CTL1 = SPI_CTL1_CP_ENABLE   |
                 SPI_CTL1_PREN_DISABLE |
                 SPI_CTL1_PTEN_DISABLE |
                 SPI_CTL1_PES_DISABLE  |
                 SPI_CTL1_MSB_ENABLE;

    SPI1->CLKCTL = 0; /* SCR=0 → 16 MHz */

    SPI1->CTL1 |= SPI_CTL1_ENABLE_ENABLE;

    GPIOB->DOUTSET31_0 = RST_PIN;
    delay_cycles(10 * 32000);
    GPIOB->DOUTCLR31_0 = RST_PIN;
    delay_cycles(20 * 32000);
    GPIOB->DOUTSET31_0 = RST_PIN;
    delay_cycles(150 * 32000);
}

/* ---- TFTInitDMA ---- */

void TFTInitDMA(void) {
    static const DL_DMA_Config cfg = {
        .transferMode  = DL_DMA_SINGLE_TRANSFER_MODE,
        .extendedMode  = DL_DMA_NORMAL_MODE,
        .destIncrement = DL_DMA_ADDR_UNCHANGED,
        .srcIncrement  = DL_DMA_ADDR_INCREMENT,
        .destWidth     = DL_DMA_WIDTH_BYTE,
        .srcWidth      = DL_DMA_WIDTH_BYTE,
        .trigger       = DMA_SPI1_TX_TRIG,
        .triggerType   = DL_DMA_TRIGGER_TYPE_EXTERNAL,
    };
    DL_DMA_initChannel(DMA, DMA_CH_SPI1_TX, (DL_DMA_Config *)&cfg);
    DL_DMA_setDestAddr(DMA, DMA_CH_SPI1_TX, (uint32_t)(&SPI1->TXDATA));

    DL_SPI_enableDMATransmitEvent(SPI1);
    DL_SPI_setFIFOThreshold(SPI1,
        DL_SPI_RX_FIFO_LEVEL_ONE_FRAME,
        DL_SPI_TX_FIFO_LEVEL_ONE_FRAME);
    DL_SPI_enableInterrupt(SPI1,
        DL_SPI_INTERRUPT_DMA_DONE_TX | DL_SPI_INTERRUPT_TX_EMPTY);
    NVIC_EnableIRQ(SPI1_INT_IRQn);

    active   = 0;
    fill_pos = 0;
    dma_busy = false;
    cs_held  = false;
}

/* ---- TFTWriteData / TFTWriteCommand / TFTReadCommand ---- */

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
        while (!(SPI1->STAT & SPI_STAT_TNF_MASK)) {}
        SPI1->TXDATA = 0x00;
        while (SPI1->STAT & SPI_STAT_RFE_MASK) {}
        out[i] = (uint8_t)(SPI1->RXDATA & 0xFF);
    }
    CSHigh();
}

/* ---- TFTFillRegion — uniform-color fill via DMA strips ---- */

void TFTFillRegion(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
    TFTWaitIdle();

    uint8_t p[4];
    CSLow();

    /* CASET */
    DCLow(); spi_txrx(0x2A);
    p[0]=x0>>8; p[1]=x0&0xFF; p[2]=x1>>8; p[3]=x1&0xFF;
    DCHigh(); SPISendBlocking(p, 4);

    /* PASET */
    DCLow(); spi_txrx(0x2B);
    p[0]=y0>>8; p[1]=y0&0xFF; p[2]=y1>>8; p[3]=y1&0xFF;
    DCHigh(); SPISendBlocking(p, 4);

    /* RAMWR */
    DCLow(); spi_txrx(0x2C); DCHigh();

    /* Stream pixel data as uniform color via double-buffered DMA strips. */
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
        tft_flush_strip(); /* wait for prev DMA, kick new, swap strip */
    }

    cs_held = true; /* CS deasserted by next TFTWaitIdle call */
}

/* ---- Streaming pixel API: begin / send / end ---- */

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
    tft_flush_strip(); /* flushes remaining bytes (or just waits if fill_pos==0) */
    cs_held = true;
}

/* ---- Hardware scroll ---- */

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

/* ---- Legacy async API (kept for compatibility; currently blocking) ---- */

void SPISetCDMode(uint8_t mode) {
    SPI1->CTL1 &= ~SPI_CTL1_CDMODE_MASK;
    SPI1->CTL1 |= (mode << SPI_CTL1_CDMODE_OFS);
}

void SPIWaitDone(void) {
    TFTWaitIdle();
}

bool SPITransferAsync(uint8_t *tx, uint8_t *rx, uint32_t len) {
    if (len == 0) return false;
    TFTWaitIdle();
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
