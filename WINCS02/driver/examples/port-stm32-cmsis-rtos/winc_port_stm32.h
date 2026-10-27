/*
 * WINCS02 nc_driver — Standalone Port File
 * This code is specific for porting the WINCS02 driver to the STM32 platform.
 * This code does not follow the regular styling conventions as it must follow
 * the contract which is given by the WINCS02 driver.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

bool WINC_PORT_SPISendReceive(void *pTransmitData, void *pReceiveData, size_t size);
void WINC_PORT_ResetAssert(void);
void WINC_PORT_ResetDeassert(void);
void WINC_PORT_DelayMs(uint32_t ms);
bool WINC_PORT_IsInterruptAsserted(void);
void WINC_PORT_DebugPrintf(const char *format, ...);
