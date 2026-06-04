# ECU6 Gateway Evidence Index

Updated: 2026-06-04 09:26 KST

## Verified by Codex in this pass

| Evidence | Path | Result |
| --- | --- | --- |
| Host-side unit tests | `Documentation/ecu6_gateway/evidence/host_unit_tests_2026-06-04.txt` | 9/9 tests passed. |
| Host test source | `Tests/host/test_ecu6_gateway_logic.py` | Covers checksum, unknown ID, alive freeze, timeout, OTA seed/key, OTA checksum, lockout. |
| Scheduler instrumentation source | `Sources/services/ms_scheduler.c` | Adds `gComScheduler10msCount`, `gComScheduler100msCount`, `gComScheduler1000msCount`, and `gComSchedulerLastCycle10ms`; requires a new S32DS build before these symbols are in the ELF. |
| Scheduler Trace32 script | `cmm/scheduler_evidence.cmm` | Ready to capture 10 ms / 100 ms / 1000 ms counter growth in Trace32. Not yet executed in Trace32. |
| Panel bridge source | `CANOE/ECU6_Panel_Bridge.can` | Replaced 0-byte placeholder with sysvar-to-OTA bridge and 0x601/0x651 panel updates. Not yet compiled/executed in CANoe. |

## Observed static artifacts

| Artifact | Path | Size | Timestamp |
| --- | --- | ---: | --- |
| S32DS Debug_FLASH ELF | `Debug_FLASH/Automotive_Cluster.elf` | 2342192 bytes | 2026-06-02 09:41:36 KST |
| S32DS Debug_FLASH map | `Debug_FLASH/Automotive_Cluster.map` | 594806 bytes | 2026-06-02 09:41:36 KST |
| Gateway CANoe binary report | `CANOE/ECU6_Gateway_Test_report.vtestreport` | 839745 bytes | 2026-06-02 16:40:44 KST |
| OTA CANoe binary report | `CANOE/ECU6_OTA_State_Test_report.vtestreport` | 864321 bytes | 2026-06-02 17:07:45 KST |

## Runtime evidence still missing

The following items were not executed by Codex and must not be treated as complete runtime evidence until the user exports/captures them:

- S32DS flash/run log or screenshot showing ECU6 ran stably for 10 minutes.
- Trace32 screenshots from `cmm/interrupt_evidence.cmm` showing `vPortTickISR`, CAN mailbox ISR, `COM_GatewayHandleRxMessage`, and `COM_GatewayBuildStatusMessage` or `CAN_SendMessage`.
- Trace32 screenshots from `cmm/scheduler_evidence.cmm` showing 10 ms / 100 ms / 1000 ms counter growth.
- CANoe readable HTML/XML/TXT report exports for `CANOE/ECU6_Gateway_Test.can` and `CANOE/ECU6_OTA_State_Test.can`.
- CANoe full six-ECU trace/report showing ECU1 `0x101`, ECU2 `0x201`, ECU3 `0x301`, ECU4 `0x401`, ECU5 `0x501`, ECU6 `0x601`, OTA request `0x650`, and OTA response `0x651`.
