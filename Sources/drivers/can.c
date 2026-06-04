/*
 * can.c
 *
 *  Created on: May 21, 2026
 *      Author: JunsangS
 */

#include "../automotive.h"

#define CAN_RX_MAILBOX_ECU1      0UL
#define CAN_RX_MAILBOX_ECU2      1UL
#define CAN_RX_MAILBOX_ECU3      2UL
#define CAN_RX_MAILBOX_ECU4      3UL
#define CAN_RX_MAILBOX_ECU5      4UL
#define CAN_TX_MAILBOX_ECU6      5UL
#define CAN_RX_MAILBOX_OTA       6UL /* Reserved for OTA request 0x650. */
#define CAN_TX_MAILBOX_OTA       7UL /* Reserved for OTA response 0x651. */
#define CAN_RX_MAILBOX_ANY       8UL /* Catch-all mailbox for blocked/unknown IDs. */

#define CAN_ID_ECU1_BODY         0x101UL
#define CAN_ID_ECU2_POWER        0x201UL
#define CAN_ID_ECU3_SENSOR       0x301UL
#define CAN_ID_ECU4_CLUSTER      0x401UL
#define CAN_ID_ECU5_DIAG         0x501UL
#define CAN_ID_OTA_REQUEST       0x650UL
#define CAN_ID_ACCEPT_ANY        0x000UL
#define CAN_STD_ID_EXACT_MASK    0x7FFUL
#define CAN_STD_ID_ACCEPT_MASK   0x000UL

#define CAN_TX_TIMEOUT_MS        10U

typedef struct
{
	uint32_t mailbox;
	uint32_t canId;
} CAN_RxMailboxConfig_t;

static const CAN_RxMailboxConfig_t gatewayRxMailboxes[] = {
	{ CAN_RX_MAILBOX_ECU1, CAN_ID_ECU1_BODY },
	{ CAN_RX_MAILBOX_ECU2, CAN_ID_ECU2_POWER },
	{ CAN_RX_MAILBOX_ECU3, CAN_ID_ECU3_SENSOR },
	{ CAN_RX_MAILBOX_ECU4, CAN_ID_ECU4_CLUSTER },
	{ CAN_RX_MAILBOX_ECU5, CAN_ID_ECU5_DIAG }
};

static const can_buff_config_t gatewayRxBuffCfg = {
	.enableFD = false,
	.enableBRS = false,
	.fdPadding = 0U,
	.idType = CAN_MSG_ID_STD,
	.isRemote = false
};

static const can_buff_config_t gatewayTxBuffCfg = {
	.enableFD = false,
	.enableBRS = false,
	.fdPadding = 0U,
	.idType = CAN_MSG_ID_STD,
	.isRemote = false
};

static can_message_t gatewayRxMessages[sizeof(gatewayRxMailboxes) / sizeof(gatewayRxMailboxes[0])];
static bool gatewayRxArmed[sizeof(gatewayRxMailboxes) / sizeof(gatewayRxMailboxes[0])];
static can_message_t otaRxMessage;
static can_message_t otaTxMessage;
static bool otaRxArmed;
static can_message_t gatewayAnyRxMessage;
static bool gatewayAnyRxArmed;

static uint8_t CAN_GetGatewayRxMailboxCount(void)
{
	return (uint8_t)(sizeof(gatewayRxMailboxes) / sizeof(gatewayRxMailboxes[0]));
}

/* Configure the RX/TX mailboxes used by ECU6 gateway communication. */
static void CAN_ConfigGatewayMailboxes(void)
{
	uint8_t i;
	status_t status;

	for (i = 0U; i < CAN_GetGatewayRxMailboxCount(); i++)
	{
		status = CAN_ConfigRxBuff(&can_pal1_instance,
		                          gatewayRxMailboxes[i].mailbox,
		                          &gatewayRxBuffCfg,
		                          gatewayRxMailboxes[i].canId);

		if (status != STATUS_SUCCESS)
		{
			LOG_Print("CAN", "ERROR", "Gateway RX mailbox config failed");
		}

		status = CAN_SetRxFilter(&can_pal1_instance,
		                         CAN_MSG_ID_STD,
		                         gatewayRxMailboxes[i].mailbox,
		                         CAN_STD_ID_EXACT_MASK);

		if (status != STATUS_SUCCESS)
		{
			LOG_Print("CAN", "ERROR", "Gateway RX exact filter config failed");
		}
	}

	status = CAN_ConfigRxBuff(&can_pal1_instance,
	                          CAN_RX_MAILBOX_OTA,
	                          &gatewayRxBuffCfg,
	                          CAN_ID_OTA_REQUEST);

	if (status != STATUS_SUCCESS)
	{
		LOG_Print("CAN", "ERROR", "OTA request RX mailbox config failed");
	}

	status = CAN_SetRxFilter(&can_pal1_instance,
	                         CAN_MSG_ID_STD,
	                         CAN_RX_MAILBOX_OTA,
	                         CAN_STD_ID_EXACT_MASK);

	if (status != STATUS_SUCCESS)
	{
		LOG_Print("CAN", "ERROR", "OTA request RX filter config failed");
	}

	status = CAN_ConfigRxBuff(&can_pal1_instance,
	                          CAN_RX_MAILBOX_ANY,
	                          &gatewayRxBuffCfg,
	                          CAN_ID_ACCEPT_ANY);

	if (status != STATUS_SUCCESS)
	{
		LOG_Print("CAN", "ERROR", "Gateway catch-all RX mailbox config failed");
	}

	/*
	 * REQ-ECU6-002: keep one lowest-priority mailbox open for every
	 * standard CAN ID so invalid IDs reach COM_GatewayHandleRxMessage().
	 */
	status = CAN_SetRxFilter(&can_pal1_instance,
	                         CAN_MSG_ID_STD,
	                         CAN_RX_MAILBOX_ANY,
	                         CAN_STD_ID_ACCEPT_MASK);

	if (status != STATUS_SUCCESS)
	{
		LOG_Print("CAN", "ERROR", "Gateway catch-all RX filter config failed");
	}

	if (CAN_ConfigTxBuff(&can_pal1_instance, CAN_TX_MAILBOX_ECU6, &gatewayTxBuffCfg) != STATUS_SUCCESS)
	{
		LOG_Print("CAN", "ERROR", "GatewayStatus TX mailbox config failed");
	}

	if (CAN_ConfigTxBuff(&can_pal1_instance, CAN_TX_MAILBOX_OTA, &gatewayTxBuffCfg) != STATUS_SUCCESS)
	{
		LOG_Print("CAN", "ERROR", "OTA response TX mailbox config failed");
	}
}

/* Send one prepared CAN message through the ECU6 TX mailbox. */
status_t CAN_SendMessage(const can_message_t *msg, uint32_t timeoutMs)
{
	return CAN_SendBlocking(&can_pal1_instance,
	                        CAN_TX_MAILBOX_ECU6,
	                        msg,
	                        timeoutMs);
}

/* Send one OTA response message through the dedicated 0x651 TX mailbox. */
status_t CAN_SendOtaResponse(const can_message_t *msg, uint32_t timeoutMs)
{
	return CAN_SendBlocking(&can_pal1_instance,
	                        CAN_TX_MAILBOX_OTA,
	                        msg,
	                        timeoutMs);
}

static status_t CAN_ReceiveGatewayMailbox(uint32_t mailbox,
                                          can_message_t *msg,
                                          uint32_t timeoutMs)
{
	return CAN_ReceiveBlocking(&can_pal1_instance,
	                           mailbox,
	                           msg,
	                           timeoutMs);
}

static status_t CAN_StartGatewayRxMailbox(uint8_t index)
{
	status_t status;

	status = CAN_Receive(&can_pal1_instance,
	                     gatewayRxMailboxes[index].mailbox,
	                     &gatewayRxMessages[index]);

	if (status == STATUS_SUCCESS)
	{
		gatewayRxArmed[index] = true;
	}

	return status;
}

static status_t CAN_StartOtaRxMailbox(void)
{
	status_t status;

	status = CAN_Receive(&can_pal1_instance,
	                     CAN_RX_MAILBOX_OTA,
	                     &otaRxMessage);

	if (status == STATUS_SUCCESS)
	{
		otaRxArmed = true;
	}

	return status;
}

static status_t CAN_StartGatewayAnyRxMailbox(void)
{
	status_t status;

	status = CAN_Receive(&can_pal1_instance,
	                     CAN_RX_MAILBOX_ANY,
	                     &gatewayAnyRxMessage);

	if (status == STATUS_SUCCESS)
	{
		gatewayAnyRxArmed = true;
	}

	return status;
}

static void CAN_StartGatewayRxMailboxes(void)
{
	uint8_t i;

	for (i = 0U; i < CAN_GetGatewayRxMailboxCount(); i++)
	{
		gatewayRxArmed[i] = false;
		(void)CAN_StartGatewayRxMailbox(i);
	}

	otaRxArmed = false;
	(void)CAN_StartOtaRxMailbox();

	gatewayAnyRxArmed = false;
	(void)CAN_StartGatewayAnyRxMailbox();
}

/* Receive one CAN message from any gateway RX mailbox. */
status_t CAN_ReceiveMessage(can_message_t *msg, uint32_t timeoutMs)
{
	uint8_t i;

	for (i = 0U; i < CAN_GetGatewayRxMailboxCount(); i++)
	{
		if (CAN_ReceiveGatewayMailbox(gatewayRxMailboxes[i].mailbox,
		                              msg,
		                              timeoutMs) == STATUS_SUCCESS)
		{
			return STATUS_SUCCESS;
		}
	}

	if (CAN_ReceiveGatewayMailbox(CAN_RX_MAILBOX_OTA,
	                              msg,
	                              timeoutMs) == STATUS_SUCCESS)
	{
		return STATUS_SUCCESS;
	}

	if (CAN_ReceiveGatewayMailbox(CAN_RX_MAILBOX_ANY,
	                              msg,
	                              timeoutMs) == STATUS_SUCCESS)
	{
		return STATUS_SUCCESS;
	}

	return STATUS_TIMEOUT;
}

static void CAN_ProcessGatewayRxMessages(void)
{
	uint8_t i;

	for (i = 0U; i < CAN_GetGatewayRxMailboxCount(); i++)
	{
		status_t status;

		if (!gatewayRxArmed[i])
		{
			(void)CAN_StartGatewayRxMailbox(i);
		}

		status = CAN_GetTransferStatus(&can_pal1_instance,
		                               gatewayRxMailboxes[i].mailbox);

		if (status == STATUS_SUCCESS)
		{
			can_message_t rxMsg = gatewayRxMessages[i];

			gatewayRxArmed[i] = false;
			(void)CAN_StartGatewayRxMailbox(i);
			COM_GatewayHandleRxMessage(&rxMsg);
		}
		else if (status != STATUS_BUSY)
		{
			gatewayRxArmed[i] = false;
		}
	}
}

static void CAN_ProcessOtaRxMessage(void)
{
	status_t status;

	if (!otaRxArmed)
	{
		(void)CAN_StartOtaRxMailbox();
	}

	status = CAN_GetTransferStatus(&can_pal1_instance, CAN_RX_MAILBOX_OTA);

	if (status == STATUS_SUCCESS)
	{
		otaRxArmed = false;

		if (OTA_HandleRequestMessage(&otaRxMessage, &otaTxMessage))
		{
			status_t txStatus = CAN_SendOtaResponse(&otaTxMessage, CAN_TX_TIMEOUT_MS);

			if (txStatus != STATUS_SUCCESS)
			{
				char message[72];

				snprintf(message,
				         sizeof(message),
				         "OTA response 0x651 TX failed: status=%d",
				         (int)txStatus);
				LOG_Print("CAN", "ERROR", message);
			}
		}

		(void)CAN_StartOtaRxMailbox();
	}
	else if (status != STATUS_BUSY)
	{
		otaRxArmed = false;
	}
}

static void CAN_ProcessGatewayAnyRxMessage(void)
{
	status_t status;

	if (!gatewayAnyRxArmed)
	{
		(void)CAN_StartGatewayAnyRxMailbox();
	}

	status = CAN_GetTransferStatus(&can_pal1_instance, CAN_RX_MAILBOX_ANY);

	if (status == STATUS_SUCCESS)
	{
		can_message_t rxMsg = gatewayAnyRxMessage;

		gatewayAnyRxArmed = false;
		(void)CAN_StartGatewayAnyRxMailbox();
		COM_GatewayHandleRxMessage(&rxMsg);
	}
	else if (status != STATUS_BUSY)
	{
		gatewayAnyRxArmed = false;
	}
}

/* Main CAN task: receive monitored frames and transmit GatewayStatus every 100 ms. */
void vCANTask(void *pvParameters)
{
	can_message_t txMsg;
	TickType_t lastGatewayTxTick;
	bool gatewayTxErrorLogged = false;
	bool gatewayTxOkLogged = false;

	(void)pvParameters;

	CAN_ConfigGatewayMailboxes();
	COM_GatewayInit();
	CAN_StartGatewayRxMailboxes();

	lastGatewayTxTick = xTaskGetTickCount();

	for (;;)
	{
		/* Poll all gateway RX mailboxes so simultaneous ECU frames are not lost. */
		CAN_ProcessGatewayRxMessages();
		CAN_ProcessOtaRxMessage();
		CAN_ProcessGatewayAnyRxMessage();

		/* Timeout supervision must run even when no new CAN frame arrives. */
		COM_GatewayCheckTimeouts();

		/* REQ-ECU6-014: ECU6 sends 0x601 GatewayStatus every 100 ms. */
		if ((xTaskGetTickCount() - lastGatewayTxTick) >= pdMS_TO_TICKS(CAN_TX_PERIOD_MS))
		{
			status_t txStatus;

			lastGatewayTxTick = xTaskGetTickCount();
			COM_GatewayBuildStatusMessage(&txMsg);
			txStatus = CAN_SendMessage(&txMsg, CAN_TX_TIMEOUT_MS);

			if (txStatus != STATUS_SUCCESS)
			{
				if (!gatewayTxErrorLogged)
				{
					char message[72];

					snprintf(message,
					         sizeof(message),
					         "GatewayStatus 0x601 TX failed: status=%d",
					         (int)txStatus);
					LOG_Print("CAN", "ERROR", message);
					gatewayTxErrorLogged = true;
				}
			}
			else
			{
				if (!gatewayTxOkLogged)
				{
					char message[96];

					snprintf(message,
					         sizeof(message),
					         "GatewayStatus TX ok: id=0x%03lx dlc=%u b0=0x%02x b1=0x%02x",
					         (unsigned long)txMsg.id,
					         (unsigned int)txMsg.length,
					         txMsg.data[0],
					         txMsg.data[1]);
					LOG_Print("CAN", "INFO", message);
					gatewayTxOkLogged = true;
				}

				if (gatewayTxErrorLogged)
				{
					LOG_Print("CAN", "INFO", "GatewayStatus 0x601 TX recovered");
					gatewayTxErrorLogged = false;
				}
			}
		}

		vTaskDelay(1U);
	}
}
