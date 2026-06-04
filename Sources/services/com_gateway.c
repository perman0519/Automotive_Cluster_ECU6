/*
 * com_gateway.c
 *
 * ECU6 gateway communication service.
 */

#include "../automotive.h"

#define CAN_ID_ECU1_BODY              0x101UL
#define CAN_ID_ECU2_POWER             0x201UL
#define CAN_ID_ECU3_SENSOR            0x301UL
#define CAN_ID_ECU4_CLUSTER           0x401UL
#define CAN_ID_ECU5_DIAG              0x501UL
#define CAN_ID_ECU6_GATEWAY           0x601UL
#define CAN_ID_OTA_REQUEST            0x650UL
#define CAN_ID_OTA_RESPONSE           0x651UL

#define CAN_GATEWAY_STATUS_DLC        8U
#define COM_PRIMARY_STATUS_DLC        8U
#define OTA_REQUEST_DLC               8U
#define OTA_RESPONSE_DLC              8U
#define OTA_RESPONSE_CHECKSUM_BYTE    7U
#define OTA_DATA_BUFFER_SIZE          256U
#define OTA_DATA_LENGTH_BYTE          2U
#define OTA_END_CHECKSUM_BYTE         2U
#define OTA_DATA_AUTH_BYTE            3U
#define OTA_DATA_PAYLOAD_OFFSET       4U
#define OTA_DATA_PAYLOAD_MAX          4U
#define COM_RX_TIMEOUT_MS             300U	//REQ-COM-005, ECU6의 timeout supervision
#define COM_ALIVE_FREEZE_LIMIT        3U	//REQ-ECU6-004
#define COM_ALIVE_COUNTER_BYTE        6U	//REQ-COM-006
#define COM_CHECKSUM_BYTE             7U	//REQ-COM-006, REQ-ECU6-003
#define COM_ALIVE_COUNTER_MASK        0x0FU	//REQ-COM-006
#define COM_CHECKSUM_RECOVERY_VALID_LIMIT  5U
#define COM_BLOCKED_LOG_INITIAL_LIMIT      5U
#define COM_BLOCKED_LOG_PERIOD             16U

#define GATEWAY_RX_MASK_ECU1          (1U << 0U)
#define GATEWAY_RX_MASK_ECU2          (1U << 1U)
#define GATEWAY_RX_MASK_ECU3          (1U << 2U)
#define GATEWAY_RX_MASK_ECU4          (1U << 3U)
#define GATEWAY_RX_MASK_ECU5          (1U << 4U)
#define GATEWAY_RX_MASK_ALL           (GATEWAY_RX_MASK_ECU1 | \
                                       GATEWAY_RX_MASK_ECU2 | \
                                       GATEWAY_RX_MASK_ECU3 | \
                                       GATEWAY_RX_MASK_ECU4 | \
                                       GATEWAY_RX_MASK_ECU5)

#define GATEWAY_STATE_INIT            0U
#define GATEWAY_STATE_NORMAL          1U
#define GATEWAY_STATE_DEGRADED        2U

#define OTA_STATE_IDLE                0U
#define OTA_STATE_START               1U
#define OTA_STATE_RECEIVING           2U
#define OTA_STATE_VERIFYING           3U
#define OTA_STATE_READY_TO_ACTIVATE   4U
#define OTA_STATE_SUCCESS             5U
#define OTA_STATE_FAILED              6U
#define ACTIVE_SW_VERSION_INITIAL     1U
#define ACTIVE_SW_VERSION_MIN         1U
#define ACTIVE_SW_VERSION_MAX         0xFFU

#define OTA_CMD_START                 0x01U
#define OTA_CMD_DATA                  0x02U
#define OTA_CMD_END                   0x03U
#define OTA_CMD_ACTIVATE              0x04U
#define OTA_CMD_ABORT                 0x05U
#define OTA_CMD_REQUEST_SEED          0x10U
#define OTA_CMD_SEND_KEY              0x11U

#define OTA_SECURITY_SEED_BASE        0x5AU
#define OTA_SECURITY_KEY_XOR          0xA5U
#define OTA_DATA_AUTH_XOR             0xC3U
#define OTA_SECURITY_LOCKOUT_LIMIT    3U
#define OTA_SECURITY_LOCKOUT_MS       3000U

#define OTA_RESPONSE_ACK              0x79U
#define OTA_RESPONSE_NACK             0x7FU
#define OTA_DETAIL_ACCEPTED           0x00U
#define OTA_DETAIL_INVALID_ID         0x10U
#define OTA_DETAIL_INVALID_DLC        0x11U
#define OTA_DETAIL_UNKNOWN_COMMAND    0x12U
#define OTA_DETAIL_INVALID_SEQUENCE   0x14U
#define OTA_DETAIL_INVALID_DATA_LENGTH 0x15U
#define OTA_DETAIL_BUFFER_OVERFLOW    0x16U
#define OTA_DETAIL_CHECKSUM_MISMATCH  0x17U
#define OTA_DETAIL_INVALID_AUTH       0x18U
#define OTA_DETAIL_SECURITY_LOCKED    0x20U
#define OTA_DETAIL_SEED_REQUIRED      0x21U
#define OTA_DETAIL_INVALID_KEY        0x22U
#define OTA_DETAIL_SECURITY_LOCKOUT   0x23U

#define GATEWAY_STATUS_OTA_MASK       0x07U					//data[1] bit0~2 = OTA state
#define GATEWAY_STATUS_SECURITY_UNLOCKED_BIT  (1U << 3U)	//data[1] bit3   = SecurityUnlocked
#define GATEWAY_STATUS_SECURITY_LOCKED_BIT    (1U << 4U)	//data[1] bit4   = SecurityLocked
#define GATEWAY_STATUS_ALIVE_ERROR_BIT        (1U << 5U)	//data[1] bit5   = AliveError
#define GATEWAY_STATUS_CHECKSUM_ERROR_BIT     (1U << 6U)	//data[1] bit6   = ChecksumError

typedef struct
{
	uint32_t canId;
	uint8_t nodeId;
	uint8_t rxMask;
	TickType_t lastRxTick;
	uint16_t timeoutMs;
	bool timeoutError;
	uint8_t lastAliveCounter;
	uint8_t aliveFreezeCnt;
	uint8_t aliveFreezeLimit;
	bool aliveError;
	bool aliveInitialized;
	bool checksumError;
	uint8_t checksumRecoveryCnt;
} ComRxMonitor_t;

//  REQ-ECU6-001 핵심 여기에 없는 ID가 들어오면 unknown ID
static ComRxMonitor_t rxMonitors[] = {
	{ CAN_ID_ECU1_BODY,   1U, GATEWAY_RX_MASK_ECU1, 0U, COM_RX_TIMEOUT_MS, false, 0U, 0U, COM_ALIVE_FREEZE_LIMIT, false, false, false, 0U },
	{ CAN_ID_ECU2_POWER,  2U, GATEWAY_RX_MASK_ECU2, 0U, COM_RX_TIMEOUT_MS, false, 0U, 0U, COM_ALIVE_FREEZE_LIMIT, false, false, false, 0U },
	{ CAN_ID_ECU3_SENSOR, 3U, GATEWAY_RX_MASK_ECU3, 0U, COM_RX_TIMEOUT_MS, false, 0U, 0U, COM_ALIVE_FREEZE_LIMIT, false, false, false, 0U },
	{ CAN_ID_ECU4_CLUSTER,4U, GATEWAY_RX_MASK_ECU4, 0U, COM_RX_TIMEOUT_MS, false, 0U, 0U, COM_ALIVE_FREEZE_LIMIT, false, false, false, 0U },
	{ CAN_ID_ECU5_DIAG,   5U, GATEWAY_RX_MASK_ECU5, 0U, COM_RX_TIMEOUT_MS, false, 0U, 0U, COM_ALIVE_FREEZE_LIMIT, false, false, false, 0U }
};

static uint8_t gatewayState = GATEWAY_STATE_INIT;
static uint8_t otaState = OTA_STATE_IDLE;
static bool securityUnlocked = false;
static bool securityLocked = true;
static uint8_t blockedMsgCnt = 0U;
static uint8_t securityErrorCounter = 0U;
static uint8_t securityFailureStreak = 0U;
static bool securityLockoutActive = false;
static TickType_t securityLockoutStartTick = 0U;
static uint8_t activeSwVersion = ACTIVE_SW_VERSION_INITIAL;
static uint8_t gatewayRxMask = 0U;
static uint8_t aliveCounter = 0U;
static uint8_t otaResponseCounter = 0U;
static uint8_t lastOtaCommand = 0U;
static bool otaVerifyPending = false;
static uint8_t securitySeed = OTA_SECURITY_SEED_BASE;
static bool securitySeedIssued = false;
static uint8_t otaDataBuffer[OTA_DATA_BUFFER_SIZE];
static uint16_t otaDataLength = 0U;
static bool otaDataOverflow = false;
static uint8_t otaExpectedChecksum = 0U;
static uint8_t otaCalculatedChecksum = 0U;
static bool otaVerifyOk = false;
static uint8_t otaExpectedAuth = 0U;
static uint8_t otaReceivedAuth = 0U;

static uint8_t COM_IncrementCounter(uint8_t *counter);
static void OTA_SetState(uint8_t nextState, const char *reason);

static uint8_t COM_GetMonitorCount(void)
{
	/* Return the number of monitored primary status messages. */
	return (uint8_t)(sizeof(rxMonitors) / sizeof(rxMonitors[0]));
}

static bool OTA_IsKnownCommand(uint8_t command)
{
	bool isKnown = false;

	switch (command)
	{
		case OTA_CMD_START:
		case OTA_CMD_DATA:
		case OTA_CMD_END:
		case OTA_CMD_ACTIVATE:
		case OTA_CMD_ABORT:
		case OTA_CMD_REQUEST_SEED:
		case OTA_CMD_SEND_KEY:
			isKnown = true;
			break;

		default:
			break;
	}

	return isKnown;
}

static uint8_t Security_CalculateKey(uint8_t seed)
{
	return (uint8_t)(seed ^ OTA_SECURITY_KEY_XOR);
}

static uint8_t Security_GenerateSeed(void)
{
	return (uint8_t)(OTA_SECURITY_SEED_BASE ^
	                 otaResponseCounter ^
	                 securityErrorCounter ^
	                 activeSwVersion);
}

static void Security_ClearFailureStreak(void)
{
	securityFailureStreak = 0U;
	securityLockoutActive = false;
	securityLockoutStartTick = 0U;
}

static void Security_ResetUnlock(void)
{
	securityUnlocked = false;
	securityLocked = true;
	securitySeedIssued = false;
}

static void Security_MarkUnlocked(void)
{
	securityUnlocked = true;
	securityLocked = false;
	securitySeedIssued = false;
	Security_ClearFailureStreak();
}

static void OTA_SetState(uint8_t nextState, const char *reason)
{
	(void)reason;
	otaState = nextState;
}

static void OTA_AdvanceActiveSwVersion(void)
{
	if (activeSwVersion >= ACTIVE_SW_VERSION_MAX)
	{
		activeSwVersion = ACTIVE_SW_VERSION_MIN;
	}
	else
	{
		activeSwVersion++;
	}
}

static void Security_UpdateLockout(void)
{
	TickType_t elapsed;

	if (securityLockoutActive)
	{
		elapsed = xTaskGetTickCount() - securityLockoutStartTick;

		if (elapsed >= pdMS_TO_TICKS(OTA_SECURITY_LOCKOUT_MS))
		{
			Security_ResetUnlock();
			Security_ClearFailureStreak();
		}
	}
}

static bool Security_IsLockoutActive(void)
{
	Security_UpdateLockout();
	return securityLockoutActive;
}

static void Security_RecordFailure(void)
{
	(void)COM_IncrementCounter(&securityErrorCounter);

	if (securityFailureStreak < OTA_SECURITY_LOCKOUT_LIMIT)
	{
		securityFailureStreak++;
	}

	if (securityFailureStreak >= OTA_SECURITY_LOCKOUT_LIMIT)
	{
		securityLockoutActive = true;
		securityLockoutStartTick = xTaskGetTickCount();
		securityUnlocked = false;
		securityLocked = true;
		securitySeedIssued = false;
		otaVerifyPending = false;
		OTA_SetState(OTA_STATE_FAILED, "security lockout");
	}
}

static void OTA_UpdateStateAfterStatusPublish(void)
{
	if (otaVerifyPending && (otaState == OTA_STATE_VERIFYING))
	{
		otaVerifyPending = false;

		if (otaVerifyOk)
		{
			OTA_SetState(OTA_STATE_READY_TO_ACTIVATE, "verification complete");
		}
		else
		{
			OTA_SetState(OTA_STATE_FAILED, "verification failed");
		}
	}
}

static void OTA_ResetDataBuffer(void)
{
	memset(otaDataBuffer, 0, sizeof(otaDataBuffer));
	otaDataLength = 0U;
	otaDataOverflow = false;
	otaExpectedChecksum = 0U;
	otaCalculatedChecksum = 0U;
	otaVerifyOk = false;
	otaExpectedAuth = 0U;
	otaReceivedAuth = 0U;
}

static uint8_t OTA_CalculateDataChecksum(void)
{
	uint8_t checksum = 0U;
	uint16_t i;

	for (i = 0U; i < otaDataLength; i++)
	{
		checksum ^= otaDataBuffer[i];
	}

	return checksum;
}

static uint8_t OTA_CalculateDataAuth(uint8_t requestSequence,
                                     uint8_t payloadLength)
{
	return (uint8_t)(securitySeed ^
	                 requestSequence ^
	                 payloadLength ^
	                 OTA_DATA_AUTH_XOR);
}

static bool OTA_VerifyDataAuth(const can_message_t *request,
                               uint8_t payloadLength,
                               uint8_t *detail)
{
	otaReceivedAuth = request->data[OTA_DATA_AUTH_BYTE];
	otaExpectedAuth = OTA_CalculateDataAuth(request->data[1], payloadLength);

	if (otaReceivedAuth != otaExpectedAuth)
	{
		*detail = OTA_DETAIL_INVALID_AUTH;
		return false;
	}

	return true;
}

static bool OTA_StoreDataPayload(const can_message_t *request,
                                 uint8_t *detail)
{
	uint8_t payloadLength = request->data[OTA_DATA_LENGTH_BYTE];
	uint8_t i;

	/* REQ-ECU6-008/012: Byte2 length, Byte3 auth, Byte4..Byte7 payload. */
	if (payloadLength > OTA_DATA_PAYLOAD_MAX)
	{
		*detail = OTA_DETAIL_INVALID_DATA_LENGTH;
		return false;
	}

	if (!OTA_VerifyDataAuth(request, payloadLength, detail))
	{
		Security_RecordFailure();
		return false;
	}

	if (((uint16_t)otaDataLength + (uint16_t)payloadLength) > OTA_DATA_BUFFER_SIZE)
	{
		otaDataOverflow = true;
		*detail = OTA_DETAIL_BUFFER_OVERFLOW;
		return false;
	}

	for (i = 0U; i < payloadLength; i++)
	{
		otaDataBuffer[otaDataLength + i] = request->data[OTA_DATA_PAYLOAD_OFFSET + i];
	}

	otaDataLength = (uint16_t)(otaDataLength + payloadLength);
	return true;
}

static bool OTA_VerifyDataChecksum(const can_message_t *request,
                                   uint8_t *detail)
{
	/* REQ-ECU6-009: OTA_END Byte2 carries the expected payload checksum. */
	otaExpectedChecksum = request->data[OTA_END_CHECKSUM_BYTE];
	otaCalculatedChecksum = OTA_CalculateDataChecksum();

	if (otaCalculatedChecksum != otaExpectedChecksum)
	{
		otaVerifyOk = false;
		*detail = OTA_DETAIL_CHECKSUM_MISMATCH;
		return false;
	}

	otaVerifyOk = true;
	return true;
}

static bool OTA_ApplyCommandState(uint8_t command,
                                  const can_message_t *request,
                                  uint8_t *detail)
{
	bool accepted = true;

	*detail = OTA_DETAIL_ACCEPTED;

	switch (command)
	{
		case OTA_CMD_START:
			if (!securityUnlocked)
			{
				accepted = false;
				*detail = OTA_DETAIL_SECURITY_LOCKED;
				break;
			}

			/* A new START command resets/restarts the OTA state flow. */
			otaVerifyPending = false;
			OTA_ResetDataBuffer();
			OTA_SetState(OTA_STATE_START, "START command");
			break;

		case OTA_CMD_DATA:
			if ((otaState == OTA_STATE_START) ||
			    (otaState == OTA_STATE_RECEIVING))
			{
				if (OTA_StoreDataPayload(request, detail))
				{
					OTA_SetState(OTA_STATE_RECEIVING, "DATA command");
				}
				else
				{
					accepted = false;
					otaVerifyPending = false;
					OTA_SetState(OTA_STATE_FAILED, "OTA data rejected");
				}
			}
			else
			{
				accepted = false;
			}
			break;

		case OTA_CMD_END:
			if ((otaState == OTA_STATE_START) ||
			    (otaState == OTA_STATE_RECEIVING))
			{
				if (OTA_VerifyDataChecksum(request, detail))
				{
					OTA_SetState(OTA_STATE_VERIFYING, "END command");
					otaVerifyPending = true;
				}
				else
				{
					accepted = false;
					otaVerifyPending = false;
					OTA_SetState(OTA_STATE_FAILED, "OTA checksum mismatch");
				}
			}
			else
			{
				accepted = false;
			}
			break;

		case OTA_CMD_ACTIVATE:
			if ((otaState == OTA_STATE_READY_TO_ACTIVATE) && otaVerifyOk)
			{
				OTA_AdvanceActiveSwVersion();
				OTA_SetState(OTA_STATE_SUCCESS, "ACTIVATE command");
			}
			else
			{
				accepted = false;
			}
			break;

		case OTA_CMD_ABORT:
			otaVerifyPending = false;
			OTA_ResetDataBuffer();
			OTA_SetState(OTA_STATE_FAILED, "ABORT command");
			break;

		default:
			accepted = false;
			break;
	}

	if (!accepted)
	{
		otaVerifyPending = false;

		if (*detail == OTA_DETAIL_ACCEPTED)
		{
			*detail = OTA_DETAIL_INVALID_SEQUENCE;
			OTA_SetState(OTA_STATE_FAILED, "invalid command sequence");
		}
	}

	return accepted;
}

/* Increment an 8-bit diagnostic counter without wrapping at 0xFF. */
static uint8_t COM_IncrementCounter(uint8_t *counter)
{
	if (*counter < 0xFFU)
	{
		(*counter)++;
	}

	return *counter;
}

/* Find the supervision entry for a received CAN ID. */
static ComRxMonitor_t *COM_FindRxMonitor(uint32_t canId)
{
	uint8_t i;

	for (i = 0U; i < COM_GetMonitorCount(); i++)
	{
		if (rxMonitors[i].canId == canId)
		{
			return &rxMonitors[i];
		}
	}

	return NULL;
}


/* Calculate the simple XOR checksum used by all primary status messages. */
static uint8_t COM_CalculateChecksum(const can_message_t *msg)
{
	uint8_t checksum = (uint8_t)(msg->id & 0xFFU);
	uint8_t i;

	/* Checksum = CAN_ID_LSB ^ data[0] ^ ... ^ data[6]. */
	for (i = 0U; i < COM_CHECKSUM_BYTE; i++)
	{
		checksum ^= msg->data[i];
	}

	return checksum;
}

static uint8_t OTA_CalculateResponseChecksum(const can_message_t *msg)
{
	uint8_t checksum = (uint8_t)(msg->id & 0xFFU);
	uint8_t i;

	for (i = 0U; i < OTA_RESPONSE_CHECKSUM_BYTE; i++)
	{
		checksum ^= msg->data[i];
	}

	return checksum;
}

static void OTA_BuildResponse(can_message_t *response,
                              uint8_t requestCommand,
                              uint8_t requestSequence,
                              uint8_t responseCode,
                              uint8_t detail)
{
	response->cs = 0U;
	response->id = CAN_ID_OTA_RESPONSE;
	response->length = OTA_RESPONSE_DLC;
	response->data[0] = responseCode;
	response->data[1] = requestCommand;
	response->data[2] = requestSequence;
	response->data[3] = detail;
	response->data[4] = (uint8_t)(otaState & GATEWAY_STATUS_OTA_MASK);
	response->data[5] = activeSwVersion;
	response->data[6] = (uint8_t)(otaResponseCounter & 0x0FU);
	response->data[7] = OTA_CalculateResponseChecksum(response);

	otaResponseCounter = (uint8_t)((otaResponseCounter + 1U) & 0x0FU);
}

/* Validate the received checksum byte against the common XOR rule. */
static bool COM_VerifyChecksum(const can_message_t *msg)
{
	/* Primary status messages must contain data[7] as checksum. */
	if (msg->length != COM_PRIMARY_STATUS_DLC)
	{
		return false;
	}

	return (msg->data[COM_CHECKSUM_BYTE] == COM_CalculateChecksum(msg));
}

/* Extract the 4-bit alive counter from the common primary status layout. */
static uint8_t COM_GetAliveCounter(const can_message_t *msg)
{
	return (uint8_t)(msg->data[COM_ALIVE_COUNTER_BYTE] & COM_ALIVE_COUNTER_MASK);
}

/* Return true when any monitored ECU has a timeout or alive-counter fault. */
static bool COM_HasAnyAliveError(void)
{
	uint8_t i;

	for (i = 0U; i < COM_GetMonitorCount(); i++)
	{
		if (rxMonitors[i].timeoutError || rxMonitors[i].aliveError)
		{
			return true;
		}
	}

	return false;
}

/* Return true after all required ECU status messages have been seen at least once. */
static bool COM_HasAllMonitoredEcusSeen(void)
{
	return ((gatewayRxMask & GATEWAY_RX_MASK_ALL) == GATEWAY_RX_MASK_ALL);
}

/* Return true when any monitored ECU has reported a checksum fault. */
static bool COM_HasAnyChecksumError(void)
{
	uint8_t i;

	for (i = 0U; i < COM_GetMonitorCount(); i++)
	{
		if (rxMonitors[i].checksumError)
		{
			return true;
		}
	}

	return false;
}

/* Update GatewayState from the current communication supervision flags. */
static void COM_UpdateGatewayState(void)
{
	if (COM_HasAnyAliveError() || COM_HasAnyChecksumError())
	{
		gatewayState = GATEWAY_STATE_DEGRADED;
	}
	else if (COM_HasAllMonitoredEcusSeen())
	{
		gatewayState = GATEWAY_STATE_NORMAL;
	}
	else
	{
		gatewayState = GATEWAY_STATE_INIT;
	}
}

/* Log the first accepted message for a monitored ECU. */
static void COM_LogAcceptedMessage(const ComRxMonitor_t *monitor)
{
	char message[72];

	snprintf(message,
	         sizeof(message),
	         "RX accepted: ECU%u CAN ID 0x%03lx",
	         monitor->nodeId,
	         (unsigned long)monitor->canId);
	LOG_Print("COM", "INFO", message);
}

static bool COM_ShouldLogBlockedMessage(uint8_t blockedCount)
{
	return ((blockedCount <= COM_BLOCKED_LOG_INITIAL_LIMIT) ||
	        ((blockedCount % COM_BLOCKED_LOG_PERIOD) == 0U));
}

/* Log an unknown CAN ID that ECU6 blocks or counts without flooding UART. */
static void COM_LogBlockedMessage(uint32_t canId, uint8_t blockedCount)
{
	char message[72];

	if (!COM_ShouldLogBlockedMessage(blockedCount))
	{
		return;
	}

	snprintf(message,
	         sizeof(message),
	         "Blocked unknown CAN ID 0x%03lx count=%u",
	         (unsigned long)canId,
	         (unsigned int)blockedCount);
	LOG_Print("COM", "WARN", message);
}

/* Log a timeout event when a monitored primary message stops arriving. */
static void COM_LogTimeout(const ComRxMonitor_t *monitor)
{
	char message[72];

	snprintf(message,
	         sizeof(message),
	         "ECU%u CAN ID 0x%03lx timeout",
	         monitor->nodeId,
	         (unsigned long)monitor->canId);
	LOG_Print("COM", "ERROR", message);
}


/* Log timeout recovery when a previously missing message is received again. */
static void COM_LogTimeoutRecovery(const ComRxMonitor_t *monitor)
{
	char message[72];

	snprintf(message,
	         sizeof(message),
	         "ECU%u CAN ID 0x%03lx timeout recovered",
	         monitor->nodeId,
	         (unsigned long)monitor->canId);
	LOG_Print("COM", "INFO", message);
}

/* Log a checksum validation failure for a monitored primary message. */
static void COM_LogChecksumError(const ComRxMonitor_t *monitor)
{
	char message[72];

	snprintf(message,
	         sizeof(message),
	         "ECU%u CAN ID 0x%03lx checksum error",
	         monitor->nodeId,
	         (unsigned long)monitor->canId);
	LOG_Print("COM", "ERROR", message);
}

/* Log checksum recovery after enough consecutive valid frames are received. */
static void COM_LogChecksumRecovery(const ComRxMonitor_t *monitor)
{
	char message[72];

	snprintf(message,
	         sizeof(message),
	         "ECU%u CAN ID 0x%03lx checksum recovered",
	         monitor->nodeId,
	         (unsigned long)monitor->canId);
	LOG_Print("COM", "INFO", message);
}

/* Log an alive-counter freeze after the configured number of repeated values. */
static void COM_LogAliveFreeze(const ComRxMonitor_t *monitor)
{
	char message[72];

	snprintf(message,
	         sizeof(message),
	         "ECU%u CAN ID 0x%03lx alive counter frozen",
	         monitor->nodeId,
	         (unsigned long)monitor->canId);
	LOG_Print("COM", "ERROR", message);
}

/* Log alive-counter recovery when the counter starts changing again. */
static void COM_LogAliveRecovery(const ComRxMonitor_t *monitor)
{
	char message[72];

	snprintf(message,
	         sizeof(message),
	         "ECU%u CAN ID 0x%03lx alive counter recovered",
	         monitor->nodeId,
	         (unsigned long)monitor->canId);
	LOG_Print("COM", "INFO", message);
}

/* Latch checksum error status for GatewayStatus and diagnostics. */
static void COM_SetChecksumError(ComRxMonitor_t *monitor)
{
	if (!monitor->checksumError)
	{
		monitor->checksumError = true;
		COM_LogChecksumError(monitor);
	}

	monitor->checksumRecoveryCnt = 0U;
}

/* Clear a checksum fault only after a short run of valid frames. */
static void COM_UpdateChecksumRecovery(ComRxMonitor_t *monitor)
{
	if (!monitor->checksumError)
	{
		return;
	}

	if (monitor->checksumRecoveryCnt < 0xFFU)
	{
		monitor->checksumRecoveryCnt++;
	}

	if (monitor->checksumRecoveryCnt >= COM_CHECKSUM_RECOVERY_VALID_LIMIT)
	{
		monitor->checksumError = false;
		monitor->checksumRecoveryCnt = 0U;
		COM_LogChecksumRecovery(monitor);
	}
}

/* Monitor whether the received 4-bit alive counter stops changing. */
static void COM_UpdateAliveMonitor(ComRxMonitor_t *monitor, const can_message_t *msg)
{
	uint8_t currentAlive = COM_GetAliveCounter(msg);

	/* The first valid frame initializes the baseline and is never a fault. */
	if (!monitor->aliveInitialized)
	{
		monitor->lastAliveCounter = currentAlive;
		monitor->aliveFreezeCnt = 0U;
		monitor->aliveInitialized = true;
		return;
	}

	/* A repeated alive value is suspicious; N repeated values become a fault. */
	if (currentAlive == monitor->lastAliveCounter)
	{
		if (monitor->aliveFreezeCnt < 0xFFU)
		{
			monitor->aliveFreezeCnt++;
		}

		if ((monitor->aliveFreezeCnt >= monitor->aliveFreezeLimit) && !monitor->aliveError)
		{
			monitor->aliveError = true;
			COM_LogAliveFreeze(monitor);
		}
	}
	else
	{
		/* Any changed alive value clears the freeze fault for this ECU. */
		if (monitor->aliveError)
		{
			COM_LogAliveRecovery(monitor);
		}

		monitor->aliveError = false;
		monitor->aliveFreezeCnt = 0U;
		monitor->lastAliveCounter = currentAlive;
	}
}

/* Initialize ECU6 gateway supervision and 0x601 status state. */
void COM_GatewayInit(void)
{
	TickType_t now = xTaskGetTickCount();
	uint8_t i;

	for (i = 0U; i < COM_GetMonitorCount(); i++)
	{
		/* Start each monitor from the current tick to avoid a startup false timeout. */
		rxMonitors[i].lastRxTick = now;
		rxMonitors[i].timeoutError = false;
		rxMonitors[i].lastAliveCounter = 0U;
		rxMonitors[i].aliveFreezeCnt = 0U;
		rxMonitors[i].aliveError = false;
		rxMonitors[i].aliveInitialized = false;
		rxMonitors[i].checksumError = false;
		rxMonitors[i].checksumRecoveryCnt = 0U;
	}

	gatewayState = GATEWAY_STATE_INIT;
	otaState = OTA_STATE_IDLE;
	securityUnlocked = false;
	securityLocked = true;
	blockedMsgCnt = 0U;
	securityErrorCounter = 0U;
	securityFailureStreak = 0U;
	securityLockoutActive = false;
	securityLockoutStartTick = 0U;
	activeSwVersion = ACTIVE_SW_VERSION_INITIAL;
	gatewayRxMask = 0U;
	aliveCounter = 0U;
	otaResponseCounter = 0U;
	lastOtaCommand = 0U;
	otaVerifyPending = false;
	securitySeed = OTA_SECURITY_SEED_BASE;
	securitySeedIssued = false;
	OTA_ResetDataBuffer();
}

/* REQ-ECU6-006: process OTA_REQUEST 0x650 and build OTA_RESPONSE 0x651. */
bool OTA_HandleRequestMessage(const can_message_t *request,
                              can_message_t *response)
{
	uint8_t requestCommand = 0U;
	uint8_t requestSequence = 0U;
	uint8_t responseCode = OTA_RESPONSE_NACK;
	uint8_t detail = OTA_DETAIL_ACCEPTED;

	if ((request == NULL) || (response == NULL))
	{
		return false;
	}

	if (request->id != CAN_ID_OTA_REQUEST)
	{
		detail = OTA_DETAIL_INVALID_ID;
		OTA_BuildResponse(response,
		                  requestCommand,
		                  requestSequence,
		                  responseCode,
		                  detail);
		return true;
	}

	if (request->length != OTA_REQUEST_DLC)
	{
		detail = OTA_DETAIL_INVALID_DLC;
		OTA_BuildResponse(response,
		                  requestCommand,
		                  requestSequence,
		                  responseCode,
		                  detail);
		return true;
	}

	requestCommand = request->data[0];
	requestSequence = request->data[1];
	lastOtaCommand = requestCommand;

	if (!OTA_IsKnownCommand(requestCommand))
	{
		detail = OTA_DETAIL_UNKNOWN_COMMAND;
		OTA_BuildResponse(response,
		                  requestCommand,
		                  requestSequence,
		                  responseCode,
		                  detail);
		return true;
	}

	if (Security_IsLockoutActive())
	{
		detail = OTA_DETAIL_SECURITY_LOCKOUT;
		OTA_BuildResponse(response,
		                  requestCommand,
		                  requestSequence,
		                  responseCode,
		                  detail);
		return true;
	}

	if (requestCommand == OTA_CMD_REQUEST_SEED)
	{
		Security_ResetUnlock();
		OTA_ResetDataBuffer();
		otaVerifyPending = false;
		OTA_SetState(OTA_STATE_IDLE, "security seed requested");
		securitySeed = Security_GenerateSeed();
		securitySeedIssued = true;
		responseCode = OTA_RESPONSE_ACK;
		OTA_BuildResponse(response,
		                  requestCommand,
		                  requestSequence,
		                  responseCode,
		                  securitySeed);
		return true;
	}

	if (requestCommand == OTA_CMD_SEND_KEY)
	{
		if (!securitySeedIssued)
		{
			detail = OTA_DETAIL_SEED_REQUIRED;
		}
		else if (request->data[2] == Security_CalculateKey(securitySeed))
		{
			Security_MarkUnlocked();
			responseCode = OTA_RESPONSE_ACK;
			detail = OTA_DETAIL_ACCEPTED;
		}
		else
		{
			Security_RecordFailure();
			Security_ResetUnlock();
			detail = OTA_DETAIL_INVALID_KEY;
		}

		OTA_BuildResponse(response,
		                  requestCommand,
		                  requestSequence,
		                  responseCode,
		                  detail);
		return true;
	}

	/* REQ-ECU6-005: update OTA state machine and expose it through 0x601. */
	if (OTA_ApplyCommandState(requestCommand, request, &detail))
	{
		responseCode = OTA_RESPONSE_ACK;
	}

	OTA_BuildResponse(response,
	                  requestCommand,
	                  requestSequence,
	                  responseCode,
	                  detail);
	return true;
}

/* Process one received CAN frame from any ECU-specific RX mailbox. */
void COM_GatewayHandleRxMessage(const can_message_t *msg)
{
	ComRxMonitor_t *monitor = COM_FindRxMonitor(msg->id);

	/* REQ-ECU6-002: unknown CAN IDs are blocked or counted. */
	if (monitor == NULL)
	{
		uint8_t blockedCount = COM_IncrementCounter(&blockedMsgCnt);

		COM_LogBlockedMessage(msg->id, blockedCount);
		return;
	}

	/* REQ-ECU6-003: allowed IDs still must pass checksum validation. */
	if (!COM_VerifyChecksum(msg))
	{
		COM_SetChecksumError(monitor);
		(void)COM_IncrementCounter(&blockedMsgCnt);
		return;
	}

	/* The message is valid and monitored, so it can update the gateway status. */
	if ((gatewayRxMask & monitor->rxMask) == 0U)
	{
		COM_LogAcceptedMessage(monitor);
	}

	gatewayRxMask |= monitor->rxMask;
	COM_UpdateChecksumRecovery(monitor);
	COM_UpdateAliveMonitor(monitor, msg);

	/* A valid frame from this ECU proves the timeout condition has recovered. */
	if (monitor->timeoutError)
	{
		monitor->timeoutError = false;
		COM_LogTimeoutRecovery(monitor);
	}

	monitor->lastRxTick = xTaskGetTickCount();
}

/* Check whether any monitored primary status message exceeded its timeout. */
void COM_GatewayCheckTimeouts(void)
{
	TickType_t now = xTaskGetTickCount();
	uint8_t i;

	for (i = 0U; i < COM_GetMonitorCount(); i++)
	{
		TickType_t elapsed = now - rxMonitors[i].lastRxTick;

		/* REQ-COM-005: missing critical input sets a latched error flag. */
		if (elapsed >= pdMS_TO_TICKS(rxMonitors[i].timeoutMs))
		{
			if (!rxMonitors[i].timeoutError)
			{
				rxMonitors[i].timeoutError = true;
				rxMonitors[i].aliveInitialized = false;
				rxMonitors[i].aliveFreezeCnt = 0U;
				COM_LogTimeout(&rxMonitors[i]);
			}
		}
	}
}


/* Build ECU6 GatewayStatus 0x601, including alive counter and checksum. */
void COM_GatewayBuildStatusMessage(can_message_t *msg)
{
	bool aliveError;

	Security_UpdateLockout();
	COM_GatewayCheckTimeouts();
	COM_UpdateGatewayState();
	aliveError = COM_HasAnyAliveError();

	/* 0x601 GatewayStatus payload layout for ECU4/CANoe decoding. */
	msg->cs = 0U;
	msg->id = CAN_ID_ECU6_GATEWAY;
	msg->length = CAN_GATEWAY_STATUS_DLC;
	msg->data[0] = gatewayState;
	msg->data[1] = (uint8_t)(otaState & GATEWAY_STATUS_OTA_MASK);
	msg->data[2] = blockedMsgCnt;
	msg->data[3] = securityErrorCounter;
	msg->data[4] = activeSwVersion;
	msg->data[5] = gatewayRxMask;
	msg->data[6] = (uint8_t)(aliveCounter & 0x0FU);
	msg->data[7] = 0U;

	/* Pack status flags into Byte1 to keep the GatewayStatus frame at DLC 8. */
	if (securityUnlocked)
	{
		msg->data[1] |= GATEWAY_STATUS_SECURITY_UNLOCKED_BIT;
	}

	if (securityLocked)
	{
		msg->data[1] |= GATEWAY_STATUS_SECURITY_LOCKED_BIT;
	}

	if (aliveError)
	{
		msg->data[1] |= GATEWAY_STATUS_ALIVE_ERROR_BIT;
	}

	if (COM_HasAnyChecksumError())
	{
		msg->data[1] |= GATEWAY_STATUS_CHECKSUM_ERROR_BIT;
	}

	/* REQ-COM-007: checksum must be calculated after all payload fields are set. */
	msg->data[7] = COM_CalculateChecksum(msg);

	/* REQ-COM-006: 4-bit alive counter increments 0..15 and wraps to 0. */
	aliveCounter = (uint8_t)((aliveCounter + 1U) & 0x0FU);

	/* Keep VERIFYING visible for one GatewayStatus frame before READY. */
	OTA_UpdateStateAfterStatusPublish();
}
