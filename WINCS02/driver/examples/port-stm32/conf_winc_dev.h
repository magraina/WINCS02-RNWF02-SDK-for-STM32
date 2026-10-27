/*
 * WINCS02 nc_driver — Standalone Configuration
 * This code is specific for adjusting the WINCS02 driver to the STM32 platform.
 * This code does not follow the regular styling conventions as it must follow
 * the contract which is given by the WINCS02 driver.
 */

#ifndef CONF_WINC_DEV_H
#define CONF_WINC_DEV_H

#include "cmsis_os2.h"

/*---------------------------------------------------------------------------
 * WINC_DEV_CACHE_LINE_SIZE
 *
 * Defines the MCU cache line size in bytes. Used to align DMA buffers to
 * cache line boundaries, avoiding corruption from cache maintenance ops.
 *
 * Set to your MCU's cache line size, or 1 if your MCU has no data cache
 * (e.g. Cortex-M0/M4/M33 without cache).
 *--------------------------------------------------------------------------*/
#define WINC_DEV_CACHE_LINE_SIZE 16

/*---------------------------------------------------------------------------
 * WINC_DEBUG_LEVEL
 *
 * Controls driver debug output verbosity. Requires calling
 * WINC_DevSetDebugPrintf() with your printf-like function at startup.
 *
 * Options:
 *   WINC_DEBUG_TYPE_NONE     - No debug output
 *   WINC_DEBUG_TYPE_ERROR    - Errors only
 *   WINC_DEBUG_TYPE_INFORM   - Errors + informational messages (recommended)
 *   WINC_DEBUG_TYPE_TRACE    - Errors + info + API call tracing
 *   WINC_DEBUG_TYPE_VERBOSE  - Everything (very noisy, useful for SPI debug)
 *--------------------------------------------------------------------------*/
#define WINC_DEBUG_LEVEL WINC_DEBUG_TYPE_INFORM

/*---------------------------------------------------------------------------
 * WINC_CONF_SPI_MOSI_IDLE_LEVEL
 *
 * Defines what the SPI MOSI line sends when idle / when pTransmitData is
 * NULL. The WINCS02 interprets 0x00 on MOSI as valid data.
 *
 *   1 = MOSI idles HIGH (0xFF sent as dummy byte) — REQUIRED for WINCS02
 *   0 = MOSI idles LOW  (driver pre-fills a TX buffer with 0xFF internally,
 *       uses more RAM)
 *
 * Set to 1 and ensure your SPI send/receive function sends 0xFF when
 * pTransmitData is NULL.
 *--------------------------------------------------------------------------*/
#define WINC_CONF_SPI_MOSI_IDLE_LEVEL 1

/*---------------------------------------------------------------------------
 * Socket Configuration
 *--------------------------------------------------------------------------*/

/* Number of sockets the driver tracks. */
#define WINC_SOCK_NUM_SOCKETS 10U

/* Socket receive buffer size in bytes. */
#define WINC_SOCK_BUF_RX_SZ (1500U * 5U)

/* Socket transmit buffer size in bytes. */
#define WINC_SOCK_BUF_TX_SZ (1500U * 5U)

/*---------------------------------------------------------------------------
 * Multi-threading / RTOS Lock Support (optional)
 *
 * If your application is single-threaded or bare-metal, leave these
 * commented out — the driver defaults to no-op lock macros via #ifndef
 * guards in winc_dev.h (lines 44-58).
 *
 * For RTOS usage, define these to use your OS mutex primitives.
 * Example for FreeRTOS:
 *
 *   #include "FreeRTOS.h"
 *   #include "semphr.h"
 *
 *   #define WINC_CONF_LOCK_STORAGE      SemaphoreHandle_t accessMutex
 *   #define WINC_CONF_LOCK_CREATE       (accessMutex = xSemaphoreCreateMutex())
 *   #define WINC_CONF_LOCK_DESTROY      vSemaphoreDelete(pCtrlCtx->accessMutex)
 *   #define WINC_CONF_LOCK_ENTER        (pdTRUE ==
 * xSemaphoreTake(pCtrlCtx->accessMutex, portMAX_DELAY)) #define
 * WINC_CONF_LOCK_LEAVE        xSemaphoreGive(pCtrlCtx->accessMutex)
 *--------------------------------------------------------------------------*/

/* #define WINC_CONF_LOCK_STORAGE       */
/* #define WINC_CONF_LOCK_CREATE        */
/* #define WINC_CONF_LOCK_DESTROY       */
/* #define WINC_CONF_LOCK_ENTER         */
/* #define WINC_CONF_LOCK_LEAVE         */

#endif /* CONF_WINC_DEV_H */
