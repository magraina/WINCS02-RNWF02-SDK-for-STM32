# WINCS02 on STM32 with CMSIS-RTOS2

Minimal example showing how to run the nc_driver under CMSIS-RTOS2 (e.g.
FreeRTOS via STM32CubeMX) and connect to an access point. It covers only what
differs from the bare-metal setup in [`port/winc_port_template.c`](../../port/winc_port_template.c).
For hardware connections, SPI requirements and the SDIO status codes see the
[driver README](../../README.md).

## Files

| File | Purpose |
|------|---------|
| `conf_winc_dev.h` | Driver configuration, lock macros mapped to a CMSIS semaphore |
| `winc_port_stm32.c/.h` | Platform functions: SPI via HAL, reset and INTOUT pins, `osDelay`, debug output |
| `wifi.c/.h` | Device init, event thread and station connect |

## Threads

```
INTOUT EXTI ──wifiNotifyFromISR()──► Event thread
                                     WINC_DevHandleEvent / WINC_DevUpdateEvent
                                       │ AEC callback (link up/down, IP)
                                       │ command callback (request complete)
                                       ▼ thread flags
                                     Connection thread
                                     init device → start event thread → connect → wait
```

- **Connection thread** resets the module, performs the SDIO handshake, starts
  the event thread and sends the station configuration (SSID, security type,
  passphrase, enable) as one command request. It then waits for link up and
  an IPv4 address. After a link loss it waits for both again.
- **Event thread** is the only place that talks to the module over SPI. It is
  woken by the INTOUT interrupt and also polls every `EVENT_POLL_MS`, because
  `WINC_DevUpdateEvent` is what actually transmits queued command requests.
- **Command buffers** come from an `osMemoryPool`. A request completes in the
  event thread, so its buffer is freed in the command callback, not by the
  sender.
- **Locking**: since the driver is called from two threads, the
  `WINC_CONF_LOCK_*` macros in `conf_winc_dev.h` must be defined. They use a
  binary CMSIS semaphore.

## Integration

1. **CubeMX**
   - SPI: mode 0 (CPOL low, CPHA 1 edge), MSB first, max 50 MHz.
   - GPIO outputs `WIFI_SPI1_CS` (idle high) and `WIFI_RESET_GPIO`.
   - GPIO input `WIFI_SPI1_IRQ` as EXTI on the falling edge.
   - Middleware: FreeRTOS with the CMSIS_V2 interface.

   The pin labels and `hspi1` are used in `winc_port_stm32.c`. Rename them
   there if your project uses different ones.

2. **Build**: add all `.c` files from `driver/src/` and from this folder. Add
   `driver/src/include/` and this folder to the include path.

3. **Credentials**: set `WIFI_SSID`, `WIFI_PASSPHRASE` and `WIFI_SEC_TYPE` at
   the top of `wifi.c`.

4. **Start** the connection thread before the scheduler:

   ```c
   osKernelInitialize();
   wifiInit();
   osKernelStart();
   ```

5. **Forward the interrupt** of the INTOUT pin:

   ```c
   void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin) {
   	if (GPIO_Pin == WIFI_SPI1_IRQ_Pin)
   		wifiNotifyFromISR();
   }
   ```

   On families without separate rising/falling callbacks use
   `HAL_GPIO_EXTI_Callback`.

6. **Debug output** goes through `vprintf`. Retarget it to a UART (e.g. via
   `_write`) or replace `WINC_PORT_DebugPrintf`. The verbosity is set by
   `WINC_DEBUG_LEVEL` in `conf_winc_dev.h`.

## Stack and memory

| Item | Size |
|------|------|
| Event thread stack | 4608 bytes (driver callbacks run here) |
| Connection thread stack | 2048 bytes |
| Receive buffer | 4096 bytes, static |
| Command pool | 2 × 512 bytes |

## Where to go from here

The connection thread marks the point where the network is usable
(`// Online`). From there you can open sockets via `winc_socket.h` or send
further commands. Use the same pattern for them: acquire a buffer from the
pool, build the request, transmit, and react to the result in the callback.
