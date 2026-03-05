
#include "tft.h"
#include "delay.h"
#include <ti/devices/msp/msp.h>

#include <stdlib.h>
#include <string.h>

// PUBLIC
// Flags for interface to main code
bool spi_wakeup;

// PRIVATE
static const uint8_t *spi_tx_buf;
static uint8_t *spi_rx_buf;
static volatile uint32_t spi_len;
static volatile uint32_t spi_idx;
static volatile bool spi_busy;

void InitializeTFT(void) {
    if (GPIOB->GPRCM.STAT & GPIO_STAT_RESETSTKY_MASK) {
        GPIOB->GPRCM.RSTCTL = (GPIO_RSTCTL_KEY_UNLOCK_W |
                                GPIO_RSTCTL_RESETSTKYCLR_CLR |
                                GPIO_RSTCTL_RESETASSERT_ASSERT);
        GPIOB->GPRCM.PWREN  = (GPIO_PWREN_KEY_UNLOCK_W |
                                GPIO_PWREN_ENABLE_ENABLE);
        delay_cycles(POWER_STARTUP_DELAY);
    } 

    // Initialize SPI0 connections!!
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM26)] = IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM26_PF_SPI1_SCLK;
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM13)] = IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM13_PF_SPI1_CS3_CD_POCI3;
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM23)] = IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM23_PF_SPI1_CS0;
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM24)] = IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM24_PF_SPI1_POCI;
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM25)] = IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM25_PF_SPI1_PICO;
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM33)] = IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM33_PF_GPIOB_DIO16;

    SPI1->GPRCM.RSTCTL = (SPI_RSTCTL_KEY_UNLOCK_W | SPI_RSTCTL_RESETSTKYCLR_CLR | SPI_RSTCTL_RESETASSERT_ASSERT);
    SPI1->GPRCM.PWREN = (SPI_PWREN_KEY_UNLOCK_W | SPI_PWREN_ENABLE_ENABLE);
    delay_cycles(POWER_STARTUP_DELAY); // delay to enable SPI to turn on and reset

    // Configure clocking for SPI0
    SPI1->CLKSEL = (uint32_t) SPI_CLKSEL_SYSCLK_SEL_ENABLE; // use the SYSOSC
    SPI1->CLKDIV = (uint32_t) SPI_CLKDIV_RATIO_DIV_BY_1; // actually 0x0, which is going to be default, but here for completeness

    // Configure the module
    SPI1->CTL0 = SPI_CTL0_SPO_LOW | SPI_CTL0_SPH_FIRST | // Clock edges and phases for data
            SPI_CTL0_PACKEN_DISABLED | 
            SPI_CTL0_CSSEL_CSSEL_0 |
            SPI_CTL0_CSCLR_ENABLE |
            SPI_CTL0_FRF_MOTOROLA_4WIRE |  // Don't use a chip select pin to bound frames
            SPI_CTL0_DSS_DSS_8;

    SPI1->CTL1 = SPI_CTL1_CP_ENABLE | // Microcontroller is CONTROLLER
            SPI_CTL1_PREN_DISABLE | SPI_CTL1_PTEN_DISABLE | SPI_CTL1_PES_DISABLE | // Disable parity on RX and TX
            SPI_CTL1_MSB_ENABLE | // Bit order is MSB first
            SPI_CTL1_CDENABLE_ENABLE;

    /* Configure Controller mode */
    /*
     * Set the bit rate clock divider to generate the serial output clock
     *     outputBitRate = (spiInputClock) / ((1 + SCR) * 2)
     *     2000000 = (32000000)/((1 + 7) * 2)
     */

    SPI1->CLKCTL = 200; // 10 bits

    // LOOK HERE!
    /* Set RX and TX FIFO threshold levels */
    SPI1->IFLS = SPI_IFLS_RXIFLSEL_LEVEL_1 | // Trigger an RX interrupt when FIFO contains >=1 sample (included for reference)
            SPI_IFLS_TXIFLSEL_LVL_EMPTY;     // Trigger an TX interrupt when the FIFO is empty

    /* Enable Transmit FIFO interrupt */
    SPI1->CPU_INT.IMASK |= SPI_CPU_INT_IMASK_TX_SET | SPI_CPU_INT_IMASK_RX_SET; // Only enable TX interrupt

    /* Enable module */
    SPI1->CTL1 |= SPI_CTL1_ENABLE_ENABLE;

    uint32_t clrPin = 1 << 16;
    
    GPIOB->DOUTCLR31_0 |= clrPin;
    delay_cycles(20 * 32000);
    GPIOB->DOUTSET31_0 |= clrPin;
    delay_cycles(150 * 32000);
}

void TFTWriteData(uint8_t *data, uint16_t len) {
    if (!data || len == 0) return;
    
    SPIWaitDone();

    SPITransferAsync(data, NULL, len);
    SPIWaitDone();
}

void TFTWriteCommand(uint8_t cmd, uint8_t *params, uint16_t len) {
    SPIWaitDone();

    SPISetCDMode(1);
    SPITransferAsync(&cmd, NULL, 1);
    SPIWaitDone();

    if (len && params) {
        SPITransferAsync(params, NULL, len);
        SPIWaitDone();
    }
}

void TFTReadCommand(uint8_t cmd, uint8_t *out, uint16_t len) {
    uint8_t dummy_tx[64];
    if (len > sizeof(dummy_tx)) return;

    memset(dummy_tx, 0x00, len);

    SPIWaitDone();

    SPISetCDMode(1);
    SPITransferAsync(&cmd, NULL, 1);
    SPIWaitDone();

    uint8_t throwaway;
    SPITransferAsync(dummy_tx, &throwaway, 1);
    SPIWaitDone();

    SPITransferAsync(dummy_tx, out, len);
    SPIWaitDone();
}

void SPISetCDMode(uint8_t mode) {
    SPI1->CTL1 |= (mode << SPI_CTL1_CDMODE_OFS);
}

void SPIWaitDone() {
    while (spi_busy) {
        __WFI();
    }
}

bool SPITransferAsync(uint8_t *tx, uint8_t *rx, uint32_t len) {
    if (spi_busy || len == 0) {
        return false;
    }

    spi_tx_buf = tx;
    spi_rx_buf = rx;
    spi_len = len;
    spi_idx = 0;
    spi_busy = true;

    NVIC_ClearPendingIRQ(SPI1_INT_IRQn);
    NVIC_EnableIRQ(SPI1_INT_IRQn);

    SPI1->TXDATA = spi_tx_buf ? spi_tx_buf[0] : 0x00;
    spi_idx = 1;

    return true;
}

void SPI1_IRQHandler(void)
{
    switch (SPI1->CPU_INT.IIDX) {
        case SPI_CPU_INT_IIDX_STAT_TX_EVT: {
            if (spi_idx < spi_len) {
                uint8_t b = spi_tx_buf ? spi_tx_buf[spi_idx] : 0x00;
                SPI1->TXDATA = b;
                spi_idx++;
            }
        } break;

        case SPI_CPU_INT_IIDX_STAT_RX_EVT: {
            volatile uint8_t r = SPI1->RXDATA;

            if (spi_rx_buf && (spi_idx <= spi_len)) {
                spi_rx_buf[spi_idx - 1] = r;
            }

            if ((spi_idx >= spi_len) && (SPI1->CPU_INT.RIS & SPI_CPU_INT_RIS_TXEMPTY_MASK)) {
                spi_busy = false;
                NVIC_DisableIRQ(SPI1_INT_IRQn);
                spi_wakeup = true;
            }
        } break;

        default:
            break;
    }
}