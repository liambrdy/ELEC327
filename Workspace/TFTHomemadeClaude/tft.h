#ifndef leds_include
#define leds_include

#include <stdbool.h>
#include <stdint.h>

extern volatile bool spi_wakeup;

void InitializeTFT(void);

/*
 * TFTInitDMA — call once after InitializeTFT + tft_init_sequence.
 * Configures DMA channel 0 for SPI1 TX, enables the SPI1 ISR, and
 * initialises the strip-buffer state.  Requires NVIC not yet in use for SPI1.
 */
void TFTInitDMA(void);

/*
 * TFTWaitIdle — block until any in-flight DMA pixel transfer is complete,
 * drain the RX FIFO, and deassert CS if it was held low by an async transfer.
 * Call before any non-pixel SPI operation if you need guaranteed ordering,
 * or use display_commit() from game code.
 */
void TFTWaitIdle(void);

bool SPITransferAsync(uint8_t *tx, uint8_t *rx, uint32_t len);
void SPIWaitDone();
void SPISetCDMode(uint8_t mode);

void TFTWriteData(uint8_t *data, uint16_t len);
void TFTWriteCommand(uint8_t cmd, uint8_t *params, uint16_t len);
void TFTReadCommand(uint8_t cmd, uint8_t *out, uint16_t len);
void TFTFillRegion(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void TFTBeginPixels(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void TFTSendPixel(uint16_t color);
void TFTEndPixels(void);

/*
 * Hardware vertical scroll (ILI9341 commands 0x33 / 0x37).
 *
 * In landscape mode (MADCTL = 0x28, MV=1) physical rows map to the logical
 * X axis, so these functions produce HORIZONTAL panning on screen:
 *
 *   TFTScrollDefine(left, width, right)
 *       left  : pixels from the left edge that stay fixed  (TFA)
 *       width : pixels that participate in the scroll       (VSA)
 *       right : pixels from the right edge that stay fixed  (BFA)
 *       Constraint: left + width + right == 320
 *
 *   TFTScrollSet(pos)
 *       pos : frame-buffer column that appears at the left edge of the
 *             scroll area (0 = normal / no shift).
 *             Incrementing pos shifts content to the LEFT on screen.
 *
 * The top/bottom HUD rows (logical Y) live in physical columns and are
 * never touched by these commands — they stay put regardless of pos.
 */
void TFTScrollDefine(uint16_t left_fixed, uint16_t scroll_width, uint16_t right_fixed);
void TFTScrollSet(uint16_t pos);

#endif // leds_include