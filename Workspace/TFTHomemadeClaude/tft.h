#ifndef leds_include
#define leds_include

#include <stdbool.h>
#include <stdint.h>

extern volatile bool spi_wakeup;

void InitializeTFT(void);
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

#endif // leds_include