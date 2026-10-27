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
 * Multi-threading / RTOS Lock Support
 *
 * Required as soon as the driver is used from more than one thread. In this
 * example commands are queued from the connection thread while the event
 * thread polls the device. The driver passes a pointer to the storage.
 *--------------------------------------------------------------------------*/

#define WINC_CONF_LOCK_STORAGE(accessMutex) osSemaphoreId_t accessMutex
#define WINC_CONF_LOCK_CREATE(accessMutex)  *(accessMutex) = osSemaphoreNew(1U, 1U, NULL)
#define WINC_CONF_LOCK_DESTROY(accessMutex) osSemaphoreDelete(*accessMutex)
#define WINC_CONF_LOCK_ENTER(accessMutex) (osOK == osSemaphoreAcquire(*accessMutex, osWaitForever))
#define WINC_CONF_LOCK_LEAVE(accessMutex) osSemaphoreRelease(*accessMutex)

#endif /* CONF_WINC_DEV_H */
