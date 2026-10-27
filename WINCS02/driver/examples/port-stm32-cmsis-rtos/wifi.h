#pragma once

#include <stdbool.h>

/// Create the connection thread. It initializes the WINCS02, starts the event
/// thread and connects to the configured access point.
/// Call before osKernelStart().
bool wifiInit(void);

/// Wake the event thread. Call from the EXTI callback of the INTOUT pin.
void wifiNotifyFromISR(void);
