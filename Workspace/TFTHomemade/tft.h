#ifndef leds_include
#define leds_include

#include <stdbool.h>
#include <stdint.h>

extern bool spi_wakeup;

void InitializeTFT(void);
bool SPITransferAsync(uint8_t *tx, uint8_t *rx, uint32_t len);
void SPIWaitDone();
void SPISetCDMode(uint8_t mode);

void TFTWriteData(uint8_t *data, uint16_t len);
void TFTWriteCommand(uint8_t cmd, uint8_t *params, uint16_t len);
void TFTReadCommand(uint8_t cmd, uint8_t *out, uint16_t len);

#endif // leds_include