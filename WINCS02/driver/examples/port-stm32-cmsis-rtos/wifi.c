#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cmsis_os2.h"
#include "microchip_wincs02_intf.h"
#include "winc_cmd_req.h"
#include "winc_cmds.h"
#include "winc_dev.h"
#include "winc_sdio_drv.h"
#include "winc_port_stm32.h"
#include "wifi.h"

#define WIFI_SSID       "my-ssid"
#define WIFI_PASSPHRASE "my-passphrase"
#define WIFI_SEC_TYPE   WINC_CFG_PARAM_OPT_WSTA_SEC_TYPE_WPA2_PERS

#define COMMAND_BUFFER_SIZE  512U
#define COMMAND_BUFFER_COUNT 2U
#define COMMAND_TIMEOUT_MS   1000U
#define EVENT_POLL_MS        10U
#define RETRY_DELAY_MS       10000U

// Event thread flags
#define FLAG_DATA_READY (1U << 0)

// Connection thread flags
#define FLAG_COMMAND_DONE   (1U << 0)
#define FLAG_COMMAND_FAILED (1U << 1)
#define FLAG_LINK_UP        (1U << 2)
#define FLAG_IP_ASSIGNED    (1U << 3)
#define FLAG_LINK_DOWN      (1U << 4)

static const osThreadAttr_t connectionThreadAttr = {
    .name = "wifiConnection", .priority = osPriorityNormal, .stack_size = 2048U};
static const osThreadAttr_t eventThreadAttr = {
    .name = "wifiEvent", .priority = osPriorityNormal, .stack_size = 4608U};

static struct {
	WINC_DEVICE_HANDLE device;
	osThreadId_t connectionThread;
	osThreadId_t eventThread;
	// Command requests complete asynchronously in the event thread, so their
	// buffers come from an RTOS pool instead of the stack.
	osMemoryPoolId_t commandPool;
} wifi = {.device = WINC_DEVICE_INVALID_HANDLE};

// Asynchronous events from the WINCS02, called from the event thread.
static void aecCallback(uintptr_t context, WINC_DEVICE_HANDLE device,
                        const WINC_DEV_EVENT_RSP_ELEMS *const elements) {
	(void)context;
	(void)device;

	if (elements == NULL)
		return;

	switch (elements->rspId) {
	case WINC_AEC_ID_WSTALU:
		osThreadFlagsSet(wifi.connectionThread, FLAG_LINK_UP);
		break;
	case WINC_AEC_ID_WSTAAIP:
		// Also reported for IPv6 addresses, only IPv4 counts as connected here.
		if (elements->numElems >= 2 && elements->elems[1].type == WINC_TYPE_IPV4ADDR)
			osThreadFlagsSet(wifi.connectionThread, FLAG_IP_ASSIGNED);
		break;
	case WINC_AEC_ID_WSTALD:
		osThreadFlagsSet(wifi.connectionThread, FLAG_LINK_DOWN);
		break;
	default:
		break;
	}
}

// Command request lifecycle, called from the event thread.
static void commandCallback(uintptr_t context, WINC_DEVICE_HANDLE device,
                            WINC_CMD_REQ_HANDLE request, WINC_DEV_CMDREQ_EVENT_TYPE event,
                            uintptr_t eventArg) {
	(void)context;
	(void)device;

	if (event != WINC_DEV_CMDREQ_EVENT_STATUS_COMPLETE)
		return;

	const WINC_DEV_EVENT_COMPLETE_ARGS *complete = (const WINC_DEV_EVENT_COMPLETE_ARGS *)eventArg;
	bool failed = complete == NULL || complete->numErrors != 0;
	osThreadFlagsSet(wifi.connectionThread, failed ? FLAG_COMMAND_FAILED : FLAG_COMMAND_DONE);

	// The request handle is the buffer itself. The driver is done with it now.
	osMemoryPoolFree(wifi.commandPool, (void *)request);
}

static void eventTask(void *argument) {
	(void)argument;

	for (;;) {
		// Woken by the INTOUT ISR. WINC_DevUpdateEvent also transmits queued
		// command requests, so it has to run periodically as well.
		osThreadFlagsWait(FLAG_DATA_READY, osFlagsWaitAny, EVENT_POLL_MS);

		if (WINC_PORT_IsInterruptAsserted())
			WINC_DevHandleEvent(wifi.device, WINC_PORT_IsInterruptAsserted);
		WINC_DevUpdateEvent(wifi.device);
	}
}

// Reset and handshake with the module. See the driver README for the
// meaning of the SDIO status codes.
static bool deviceInit(void) {
	static uint8_t receiveBuffer[4096];
	static WINC_DEV_INIT devInit = {.pReceiveBuffer = receiveBuffer,
	                                .receiveBufferSize = sizeof(receiveBuffer)};

	WINC_DevSetDebugPrintf(WINC_PORT_DebugPrintf);

	WINC_PORT_ResetAssert();
	WINC_PORT_DelayMs(100);
	WINC_PORT_ResetDeassert();

	wifi.device = WINC_DevInit(&devInit);
	if (wifi.device == WINC_DEVICE_INVALID_HANDLE)
		return false;

	// The module needs ~3.5 s to boot, 100 attempts give it ~10 s.
	WINC_SDIO_STATE_TYPE sdioState = WINC_SDIO_STATE_UNKNOWN;
	WINC_SDIO_STATUS_TYPE sdioStatus = WINC_SDIO_STATUS_UNKNOWN;
	for (int attempt = 0; attempt < 100; attempt++) {
		sdioStatus = WINC_SDIODeviceInit(&sdioState, WINC_PORT_SPISendReceive);
		if (sdioStatus == WINC_SDIO_STATUS_OK)
			break;

		if (sdioStatus == WINC_SDIO_STATUS_OP_FAILED)
			sdioState = WINC_SDIO_STATE_UNKNOWN;
		else if (sdioStatus != WINC_SDIO_STATUS_RESET_WAITING &&
		         sdioStatus != WINC_SDIO_STATUS_OP_WAITING)
			break; // Unrecoverable, check SPI wiring and power

		WINC_PORT_DelayMs(100);
	}

	if (sdioStatus != WINC_SDIO_STATUS_OK) {
		WINC_DevDeinit(wifi.device);
		wifi.device = WINC_DEVICE_INVALID_HANDLE;
		return false;
	}

	WINC_DevBusStateSet(wifi.device, WINC_DEV_BUS_STATE_ACTIVE);
	WINC_DevAECCallbackRegister(wifi.device, aecCallback, 0);
	return true;
}

// Configure the station and start connecting. Blocks until the module has
// accepted the commands, the connection itself is reported via AEC.
static bool connect(void) {
	uint8_t *buffer = osMemoryPoolAlloc(wifi.commandPool, 0U);
	if (buffer == NULL)
		return false;

	WINC_CMD_REQ_HANDLE request =
	    WINC_CmdReqInit(buffer, COMMAND_BUFFER_SIZE, 4, commandCallback, 0);
	if (request == WINC_CMD_REQ_INVALID_HANDLE) {
		osMemoryPoolFree(wifi.commandPool, buffer);
		return false;
	}

	WINC_CmdWSTAC(request, WINC_CFG_PARAM_ID_WSTA_SSID, WINC_TYPE_STRING, (uintptr_t)WIFI_SSID,
	              strlen(WIFI_SSID));
	WINC_CmdWSTAC(request, WINC_CFG_PARAM_ID_WSTA_SEC_TYPE, WINC_TYPE_INTEGER, WIFI_SEC_TYPE,
	              sizeof(uint32_t));
	WINC_CmdWSTAC(request, WINC_CFG_PARAM_ID_WSTA_CREDENTIALS, WINC_TYPE_STRING,
	              (uintptr_t)WIFI_PASSPHRASE, strlen(WIFI_PASSPHRASE));
	WINC_CmdWSTA(request, WINC_CONST_WSTA_STATE_ENABLE);

	if (!WINC_DevTransmitCmdReq(wifi.device, request)) {
		osMemoryPoolFree(wifi.commandPool, buffer);
		return false;
	}

	uint32_t flags = osThreadFlagsWait(FLAG_COMMAND_DONE | FLAG_COMMAND_FAILED, osFlagsWaitAny,
	                                   COMMAND_TIMEOUT_MS);
	return (flags & osFlagsError) == 0 && (flags & FLAG_COMMAND_FAILED) == 0;
}

static void connectionTask(void *argument) {
	(void)argument;

	// Blocking delays during init are fine here, they only pause this thread.
	while (!deviceInit())
		osDelay(RETRY_DELAY_MS);

	// Started only now, so it never polls an uninitialized device.
	wifi.eventThread = osThreadNew(eventTask, NULL, &eventThreadAttr);
	if (wifi.eventThread == NULL)
		osThreadExit();

	while (!connect())
		osDelay(RETRY_DELAY_MS);

	for (;;) {
		// Link up and IP assignment arrive as separate events. After a link
		// loss the module reconnects on its own, so we just wait again.
		osThreadFlagsWait(FLAG_LINK_UP | FLAG_IP_ASSIGNED, osFlagsWaitAll, osWaitForever);

		// Online: the network can be used from here on.

		osThreadFlagsWait(FLAG_LINK_DOWN, osFlagsWaitAny, osWaitForever);
	}
}

bool wifiInit(void) {
	wifi.commandPool = osMemoryPoolNew(COMMAND_BUFFER_COUNT, COMMAND_BUFFER_SIZE, NULL);
	wifi.connectionThread = osThreadNew(connectionTask, NULL, &connectionThreadAttr);
	return wifi.commandPool != NULL && wifi.connectionThread != NULL;
}

void wifiNotifyFromISR(void) {
	if (wifi.eventThread != NULL)
		osThreadFlagsSet(wifi.eventThread, FLAG_DATA_READY);
}
