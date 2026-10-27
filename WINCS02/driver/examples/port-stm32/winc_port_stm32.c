/*
 * WINCS02 nc_driver — Standalone Port File
 * This code is specific for porting the WINCS02 driver to the STM32 platform.
 * This code does not follow the regular styling conventions as it must follow
 * the contract which is given by the WINCS02 driver.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "cmsis_os2.h"
#include "console/console.h"
#include "spi.h"
#include "winc_port_stm32.h"

#define SPI_TIMEOUT 1000
#define CHUNK_SIZE 256

static inline size_t min(size_t a, size_t b) { return (a < b) ? a : b; }

/*----------------------------------------------------------------------------
 * SPI Send/Receive  (REQUIRED)
 *
 * This is the core platform function. The nc_driver calls it via a function
 * pointer of type WINC_SDIO_SEND_RECEIVE_FP (defined in winc_sdio_drv.h):
 *
 *   typedef bool (*WINC_SDIO_SEND_RECEIVE_FP)(
 *       void* pTransmitData,
 *       void* pReceiveData,
 *       size_t size
 *   );
 *
 * Requirements:
 *   - SPI Mode 0 (CPOL=0, CPHA=0), MSB first
 *   - Maximum SPI clock: 50 MHz (refer to WINCS02 datasheet Table 3-11;
 *     actual max may be lower depending on PCB layout)
 *   - CS must be ASSERTED (driven low) at the START of each call
 *   - CS must be DEASSERTED (driven high) at the END of each call
 *   - When pTransmitData is NULL, transmit 0xFF for each byte
 *     (the WINCS02 interprets 0x00 on MOSI as valid data)
 *   - When pReceiveData is NULL, discard received bytes
 *   - Return true on success, false on failure
 *
 * This function is passed to WINC_SDIODeviceInit() during initialisation.
 *--------------------------------------------------------------------------*/
bool WINC_PORT_SPISendReceive(void *pTransmitData, void *pReceiveData,
                              size_t size) {
  uint8_t *pTx = (uint8_t *)pTransmitData;
  uint8_t *pRx = (uint8_t *)pReceiveData;
  HAL_StatusTypeDef spiStatus = HAL_OK;
  static const uint8_t txDummyBlock[] = {[0 ... CHUNK_SIZE - 1] = 0xFFU};

  // Assert CS
  HAL_GPIO_WritePin(WIFI_SPI1_CS_GPIO_Port, WIFI_SPI1_CS_Pin, GPIO_PIN_RESET);

  if (pTx != NULL && pRx != NULL) {
    spiStatus = HAL_SPI_TransmitReceive(&hspi1, pTx, pRx, size, SPI_TIMEOUT);
  } else {
    for (size_t offset = 0; offset < size; offset += CHUNK_SIZE) {
      size_t currentSize = min(size - offset, CHUNK_SIZE);

      if (pTx == NULL) {
        spiStatus = HAL_SPI_TransmitReceive(&hspi1, txDummyBlock, pRx + offset,
                                            currentSize, SPI_TIMEOUT);
      } else {
        static uint8_t rxDummyBlock[CHUNK_SIZE];
        spiStatus = HAL_SPI_TransmitReceive(&hspi1, pTx + offset, rxDummyBlock,
                                            currentSize, SPI_TIMEOUT);
      }

      if (spiStatus != HAL_OK) {
        break;
      }
    }
  }

  // Deassert CS
  HAL_GPIO_WritePin(WIFI_SPI1_CS_GPIO_Port, WIFI_SPI1_CS_Pin, GPIO_PIN_SET);

  return (spiStatus == HAL_OK);
}

/*----------------------------------------------------------------------------
 * MCLR Reset Pin Control  (REQUIRED)
 *
 * The MCLR pin is active-low. Driving it low puts the WINCS02 into reset.
 * Releasing it high allows the module to boot.
 *--------------------------------------------------------------------------*/

void WINC_PORT_ResetAssert(void) {
  HAL_GPIO_WritePin(WIFI_RESET_GPIO_GPIO_Port, WIFI_RESET_GPIO_Pin,
                    GPIO_PIN_RESET);
}

void WINC_PORT_ResetDeassert(void) {
  HAL_GPIO_WritePin(WIFI_RESET_GPIO_GPIO_Port, WIFI_RESET_GPIO_Pin,
                    GPIO_PIN_SET);
}

/*----------------------------------------------------------------------------
 * INTOUT Pin Read  (REQUIRED)
 *
 * The WINCS02 INTOUT pin (active-low, push-pull) signals when the module
 * has data for the host to read. Check this pin in your main loop.
 *--------------------------------------------------------------------------*/

bool WINC_PORT_IsInterruptAsserted(void) {
  return (HAL_GPIO_ReadPin(WIFI_SPI1_IRQ_GPIO_Port, WIFI_SPI1_IRQ_Pin) ==
          GPIO_PIN_RESET);
}

/*----------------------------------------------------------------------------
 * Delay  (REQUIRED)
 *
 * Blocking millisecond delay used during initialisation.
 *--------------------------------------------------------------------------*/

void WINC_PORT_DelayMs(uint32_t ms) { osDelay(ms); }

/*----------------------------------------------------------------------------
 * Debug Printf  (OPTIONAL but highly recommended)
 *
 * Route driver debug output to your serial console (UART, SWO, etc.).
 * Call WINC_DevSetDebugPrintf() with this function during init.
 * Set WINC_DEBUG_LEVEL in conf_winc_dev.h to control verbosity.
 *--------------------------------------------------------------------------*/

void WINC_PORT_DebugPrintf(const char *format, ...) {
  va_list args;
  va_start(args, format);
  printf(format, args);
  va_end(args);
}
