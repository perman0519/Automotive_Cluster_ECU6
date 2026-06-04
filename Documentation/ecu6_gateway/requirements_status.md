# ECU6 Requirements Status

Monitoring time: 2026-06-04 09:26 KST

Source requirement document: `C:\Users\JunsangS\Downloads\MPC5748G_Embedded_Training_Requirements_EN.docx`

Build status: not executed by Codex. User-side S32DS `Debug_FLASH` build artifacts were observed on 2026-06-02 09:41 KST; flash/runtime execution was not verified by Codex.

## Scope

The requirement document contains 96 total requirements:

| Group | Count |
| --- | ---: |
| Common | 15 |
| ECU1 | 10 |
| ECU2 | 10 |
| ECU3 | 10 |
| ECU4 | 12 |
| ECU5 | 14 |
| ECU6 | 16 |
| Integration | 9 |

This project is currently ECU6-focused. The active monitoring scope is:

- 15 common requirements.
- 16 ECU6 requirements.
- 4 ECU6-related integration requirements: REQ-INT-001, REQ-INT-007, REQ-INT-008, REQ-INT-009.

Active scope count: 35 requirements.

## Summary

| Status | Count | Meaning |
| --- | ---: | --- |
| Complete | 31 | Code/document/test asset exists for the requirement; runtime evidence may still need to be collected in S32DS/CANoe/Trace32. |
| Partial | 4 | Some implementation or document evidence exists, but the requirement is not fully closed. |
| Missing | 0 | No meaningful implementation found for this project yet. |

Remaining active requirements: 4.

Remaining mandatory P0 requirements: 4.

## Latest Monitor Notes

- 2026-06-04 09:26 KST update: host-side unit tests were added under `Tests/host/` and executed with the bundled Python runtime. Result evidence is recorded in `Documentation/ecu6_gateway/evidence/host_unit_tests_2026-06-04.txt`; all 9 tests passed, so REQ-COM-015 is now Complete.
- `Sources/services/ms_scheduler.c` now exposes Trace32-visible scheduler counters: `gComScheduler10msCount`, `gComScheduler100msCount`, `gComScheduler1000msCount`, and `gComSchedulerLastCycle10ms`. `cmm/scheduler_evidence.cmm` was added, and `cmm/test_debug.cmm` / `cmm/interrupt_evidence.cmm` watch lists were updated. A new S32DS build is required before these symbols are present in `Debug_FLASH/Automotive_Cluster.elf`; REQ-COM-004 remains Partial until Trace32 runtime screenshots show counter growth.
- `CANOE/ECU6_Panel_Bridge.can` is no longer a 0-byte placeholder. It now bridges ECU6 panel sysvars to OTA request `0x650` commands and updates panel sysvars from `0x601`/`0x651`. CANoe compile/execution evidence is still pending, so REQ-INT-001 remains Partial.
- `Documentation/ecu6_gateway/evidence/evidence_index.md` now separates verified host evidence from static artifacts and missing S32DS/CANoe/Trace32 runtime evidence.
- `Debug_FLASH/Automotive_Cluster.elf` and `Debug_FLASH/Automotive_Cluster.map` are present, but Codex did not flash/run the target or observe a 10-minute stable runtime. REQ-COM-001 remains Partial.
- `Configuration1.cfg` references ECU1, ECU2, ECU3, ECU4, ECU5, ECU6 gateway test, ECU6 OTA test, and ECU6 panel bridge CAPL sources. Its active database reference observed by text search is `CANOE\ECU5_UDS 1.dbc`, not the reviewed `CANOE/ECU6_Gateway_OTA.dbc`; this should be reviewed in CANoe before using the configuration as final REQ-COM-003/REQ-INT-001 evidence.
- Build was not executed by Codex. User-side S32DS `Debug_FLASH/Automotive_Cluster.elf` and `.map` artifacts were observed with 2026-06-02 09:41 KST timestamps.
- Requirement counts are unchanged from the previous check.
- No new source/CANoe/Trace32/build evidence was found after the 2026-06-03 17:55 KST monitor pass; this run only refreshed `Documentation/ecu6_gateway/requirements_status.md`.
- New CANoe report/configuration artifacts were found after the 16:04 KST monitor pass: updated `Configuration1.cfg`, `Configuration1.stcfg`, `CANOE/ECU6_Gateway_Test_report.vtestreport`, `CANOE/ECU6_OTA_State_Test_report.vtestreport`, `CANOE/ECU6_Gateway_OTA_merged.dbc`, `CANOE/6_ECUs_dbc_0206.dbc`, `CANOE/6_ECUs_dbc_0206.ini`, and updated `.cbf` files for ECU4, gateway, OTA, and panel bridge nodes.
- The updated CANoe `.vtestreport` files did not expose readable test names or pass/fail verdicts through text search, so they are recorded as updated report artifacts, not closed runtime verdict evidence.
- Historical note: at the 16:04 KST monitor pass, `Configuration1.cfg` referenced the updated CANoe report files, `CANOE/ECU4.can`, and `CANOE/ECU6_Panel_Bridge.cbf`, while `CANOE/ECU6_Panel_Bridge.can` was still empty. This was addressed on 2026-06-04 by adding minimal bridge logic; CANoe compile/execution evidence is still pending.
- `CANOE/6_ECUs_dbc_0206.dbc` and `CANOE/ECU6_Gateway_OTA_merged.dbc` document the six-ECU status, gateway, OTA, and UDS frame set; they were not found as the active DBC reference in `Configuration1.cfg` during this pass.
- New Trace32/CANoe support artifacts were found after the 10:58 KST monitor pass: `cmm.7z`, `cmm/startup_dashboard.cmm`, `cmm/automotive_toolbar.cmm`, `cmm/interrupt_evidence.cmm`, `cmm/show_main.cmm`, `cmm/show_registers.cmm`, updated gateway/OTA/security CMM scripts, `CANOE/ECU4.can`, `CANOE/ECU5_UDS.ini`, and `CANOE/ECU6_UDS_FINAL.dbc`.
- `cmm/interrupt_evidence.cmm` explicitly defines screenshot steps for `vPortTickISR`, CAN mailbox ISR, `COM_GatewayHandleRxMessage`, and `COM_GatewayBuildStatusMessage` evidence. It is a useful REQ-COM-001/REQ-COM-003/REQ-ECU6-014 evidence collection script, but the actual Trace32 screenshots are still pending.
- `CANOE/ECU4.can` now provides a 100 ms `ClusterStatus` (`0x401`) simulator with alive counter and XOR checksum, strengthening CANoe integration setup evidence for ECU6 monitoring of ECU4.
- `CANOE/ECU6_UDS_FINAL.dbc` has the same content hash as `CANOE/ECU5_UDS.dbc` and `CANOE/ECU6_Gateway_OTA_ClusterStatus_modified.dbc`; `Configuration1.cfg` still references the reviewed primary `CANOE/ECU6_Gateway_OTA.dbc`.
- New CANoe/configuration evidence was found after the 08:58 KST monitor pass: updated `Configuration1.cfg`, `Configuration1.stcfg`, `CANOE/Panel1.xvp`, `CANOE/ECU6_Gateway_OTA.ini`, `CANOE/ECU6_Gateway_OTA_ClusterStatus_modified.dbc`, `CANOE/ECU6_Gateway_OTA_ClusterStatus_modified.ini`, `CANOE/ECU5_UDS.dbc`, and `CANOE/ECU6_Panel_Bridge.can`.
- Historical note: at the 08:58 KST monitor pass, `Configuration1.cfg` referenced `CANOE/Panel1.xvp` and an empty `CANOE/ECU6_Panel_Bridge.can`; this placeholder was replaced with bridge logic on 2026-06-04.
- `CANOE/ECU5_UDS.dbc` and `CANOE/ECU6_Gateway_OTA_ClusterStatus_modified.dbc` have matching contents and document `ClusterStatus`, `DiagnosticStatus`, `GatewayStatus`, OTA, and UDS frames. `Configuration1.cfg` still references the reviewed primary `CANOE/ECU6_Gateway_OTA.dbc`.
- These new build/configuration artifacts strengthen documentation and integration-setup evidence, but they do not close the remaining runtime gaps because CANoe verdicts, Trace32 evidence, 10-minute run evidence, and full integration execution evidence are still pending.
- New CANoe configuration/database evidence was found after the 14:12 KST monitor pass: `Configuration1.cfg`, `Configuration1.stcfg`, `CANOE/ECU6_Gateway_OTA.dbc`, `CANOE/ECU6_Gateway_OTA.ini`, updated `.cbf` files, and updated `CANOE/ECU3.can`.
- `Configuration1.cfg` now references `CANOE/ECU6_Gateway_OTA.dbc`, so the CANoe configuration is tied to the reviewed ECU6 gateway/OTA database instead of leaving the DBC as a standalone artifact.
- `CANOE/ECU6_Gateway_OTA.dbc` was expanded with UDS request/response frames (`0x7DF`, `0x7E0`, `0x7E8`), message cycle-time attributes, valid-range comments, and a more detailed ECU3 `SensorStatus` layout. `CANOE/ECU3.can` matches the updated ECU3 layout with 16-bit steering angle, packed status flags, alive counter, and checksum.
- `CANOE/ECU6_OTA_State_Test_report.vtestreport` was modified, but the binary report format still did not expose readable pass/fail verdicts through text search; runtime verdict evidence remains pending until reviewed/exported.
- New CAN database artifacts were found after the 09:11 KST monitor pass: `CANOE/ECU6_Gateway_OTA.dbc`, `CANOE/ECUDBC.dbc`, and `CANOE/ECUDBC.ini`.
- `CANOE/ECU6_Gateway_OTA.dbc` defines ECU1..ECU5 monitored status frames, ECU6 `GatewayStatus` (`0x601`), `OTA_Request` (`0x650`), and `OTA_Response` (`0x651`). The reviewed `GatewayStatus` bit layout matches `COM_GatewayBuildStatusMessage()`.
- In the 11:42 KST run, no new source/Trace32/build evidence was found.
- `CANOE/ECU6_Gateway_Test_report.vtestreport` and `CANOE/ECU6_OTA_State_Test_report.vtestreport` exist, but the current binary report format did not expose readable pass/fail verdicts during this monitor run; runtime evidence remains pending until the verdicts are reviewed or exported.
- Requirement counts now include REQ-ECU6-008 as complete at code-review/test-asset level.
- `Sources/services/com_gateway.c` now implements seed/key security commands `REQUEST_SEED` (`0x10`) and `SEND_KEY` (`0x11`) using `seed XOR 0xA5`.
- `OTA_CMD_START` is now rejected while security is locked, closing the OTA_START security gate requirement at code-review level.
- `OTA_CMD_DATA` now verifies Byte3 AuthByte, stores Byte4..Byte7 payload bytes into a 256-byte bounded buffer using Byte2 as payload length, and overflow is rejected with NACK detail `0x16`.
- `OTA_CMD_END` now verifies the stored OTA payload using an 8-bit XOR checksum from Byte2; mismatch returns NACK detail `0x17` and OTA_FAILED.
- Wrong OTA_DATA AuthByte returns NACK detail `0x18`, increments `SecurityErrorCounter`, and moves OTA state to FAILED.
- Three wrong key/AuthByte security attempts now activate a 3000 ms lockout. During lockout, known OTA commands return NACK detail `0x23`; after timeout, `REQUEST_SEED` is accepted again.
- `OTA_CMD_ACTIVATE` now advances `ActiveSWVersion` only when OTA is READY_TO_ACTIVATE and checksum verification succeeded. Version wraps from `0xFF` to `0x01`.
- `CANOE/ECU6_Gateway_Test.can` and `CANOE/ECU6_OTA_State_Test.can` were updated to unlock security before OTA state tests and to check GatewayStatus security flags/error counter.
- `CANOE/ECU6_OTA_State_Test.can` now includes 100-byte normal transfer and 256-byte overflow rejection tests for REQ-ECU6-008.
- `CANOE/ECU6_OTA_State_Test.can` now includes checksum success/failure tests for REQ-ECU6-009 and REQ-INT-008.
- `CANOE/ECU6_OTA_State_Test.can` now includes wrong AuthByte rejection coverage for REQ-ECU6-012, lockout/timeout coverage for REQ-ECU6-013, and ActiveSWVersion update coverage for REQ-ECU6-015.
- `cmm/security_debug.cmm` now watches seed/key security state variables and lockout variables.
- `cmm/ota_debug.cmm` now watches OTA data length, overflow status, buffer contents, expected/calculated checksums, AuthByte values, and verify result.
- `Sources/services/system_init.c` now clears PC4/LED4 once on `CAN_Init()` success; the previous duplicate clear call was removed.
- No major ECU6 OTA/security implementation blocker remains at code-review level. Runtime evidence is still pending.

## Detailed Status

| Requirement | Status | Evidence / gap |
| --- | --- | --- |
| REQ-COM-001 | Partial | Clock/GPIO/UART/CAN init and FreeRTOS tasks exist; CAN init success clears PC4/LED4 once. `Debug_FLASH/Automotive_Cluster.elf` and `.map` exist, and `cmm/interrupt_evidence.cmm` is ready, but Codex did not execute S32DS flash/run or capture 10-minute runtime/interrupt screenshots. Blocked by missing runtime evidence. |
| REQ-COM-002 | Complete | `LOG_Print()` emits `[time][ECU6][module][level] message`; gateway/OTA/CAN logs exist. |
| REQ-COM-003 | Partial | ECU6 transmits `0x601` and receives monitored IDs/OTA request in code. ECU1..ECU5 CAPL simulator assets and CANoe test assets exist, and binary `.vtestreport` files were observed. Readable CANoe verdict/export evidence for monitored ID RX, `0x601` TX, `0x650` RX, `0x651` TX, unknown ID, checksum, alive, and timeout verdicts is still missing; `Configuration1.cfg` DBC reference also needs CANoe review. |
| REQ-COM-004 | Partial | `vComSchedulerTask()` now increments Trace32-visible 10 ms / 100 ms / 1000 ms counters, and `cmm/scheduler_evidence.cmm` was added. A new S32DS build is required to place these new symbols in the ELF; Complete status still requires Trace32/CANoe runtime evidence showing counter growth or equivalent scheduler execution timing. |
| REQ-COM-005 | Complete | `COM_GatewayCheckTimeouts()` detects monitored message timeout and reports/logs it. |
| REQ-COM-006 | Complete | `COM_GatewayBuildStatusMessage()` packs a 4-bit alive counter in byte 6. |
| REQ-COM-007 | Complete | XOR checksum is implemented and validated for monitored messages and 0x601. |
| REQ-COM-008 | Complete | Source is separated into drivers and services; `main.c` is not overloaded. |
| REQ-COM-009 | Complete | Gateway state, OTA command state, and security locked/unlocked/seed-issued behavior are implemented and testable. |
| REQ-COM-010 | Complete | Trace32 scripts exist for init, runtime review, gateway, OTA, security, startup dashboard, toolbar, register/source display, and interrupt evidence capture. |
| REQ-COM-011 | Complete | `CANOE/ECU6_Gateway_Test.can` contains more than three ECU6 CAPL tests. Execution result is pending. |
| REQ-COM-012 | Complete | `Documentation/ecu6_gateway/design.md` and `Documentation/ecu6_gateway/test_report.md` were created. |
| REQ-COM-013 | Complete | Timeout, checksum, and alive fault recovery paths exist. |
| REQ-COM-014 | Complete | Requirement IDs and traceable names are present in source/CMM/docs. |
| REQ-COM-015 | Complete | Host-side unit tests were added in `Tests/host/` and executed with bundled Python. Evidence: `Documentation/ecu6_gateway/evidence/host_unit_tests_2026-06-04.txt`; 9/9 tests passed for checksum, unknown ID, alive freeze, timeout, seed/key unlock, OTA checksum, and security lockout logic. |
| REQ-ECU6-001 | Complete | Monitors allowed IDs `0x101`, `0x201`, `0x301`, `0x401`, `0x501`. |
| REQ-ECU6-002 | Complete | Unknown IDs are counted through `BlockedMsgCnt`. |
| REQ-ECU6-003 | Complete | Checksum validation sets error status and increments blocked/error count. |
| REQ-ECU6-004 | Complete | Alive counter freeze detection is implemented. Specific CAPL freeze test still should be added. |
| REQ-ECU6-005 | Complete | OTA states IDLE, START, RECEIVING, VERIFYING, READY_TO_ACTIVATE, SUCCESS, FAILED are implemented and exposed through GatewayStatus. |
| REQ-ECU6-006 | Complete | OTA request `0x650` and response `0x651` transport is implemented. |
| REQ-ECU6-007 | Complete | START/DATA/END/ACTIVATE/ABORT command-state transitions and invalid-sequence NACK handling are implemented. |
| REQ-ECU6-008 | Complete | `OTA_CMD_DATA` stores up to 256 bytes in a bounded buffer; overflow is rejected with NACK detail `0x16` and covered by CAPL test assets. |
| REQ-ECU6-009 | Complete | `OTA_CMD_END` verifies an 8-bit XOR checksum over stored OTA data before READY_TO_ACTIVATE; wrong checksum returns NACK detail `0x17` and FAILED. |
| REQ-ECU6-010 | Complete | OTA_START is rejected with security-locked detail before seed/key unlock. |
| REQ-ECU6-011 | Complete | REQUEST_SEED and SEND_KEY are implemented using a simple seed XOR key rule; wrong key increments SecurityErrorCounter. |
| REQ-ECU6-012 | Complete | `OTA_CMD_DATA` validates AuthByte as `seed XOR sequence XOR payloadLength XOR 0xC3`; wrong auth returns NACK detail `0x18` and increments `SecurityErrorCounter`. |
| REQ-ECU6-013 | Complete | Three wrong key/AuthByte security attempts activate lockout, reject known OTA commands with detail `0x23`, and clear after a 3000 ms timeout. CAPL test asset exists. |
| REQ-ECU6-014 | Complete | `GatewayStatus` `0x601` is built and sent every 100 ms in `vCANTask()`. |
| REQ-ECU6-015 | Complete | `ActiveSWVersion` increments only after verified READY_TO_ACTIVATE + ACTIVATE success, and CAPL checks it does not change before activation or on invalid activation. |
| REQ-ECU6-016 | Complete | gateway_debug.cmm, ota_debug.cmm, security_debug.cmm, startup_dashboard.cmm, and interrupt_evidence.cmm exist and watch or guide capture for key gateway/OTA/security/interrupt variables. |
| REQ-INT-001 | Partial | ECU1..ECU5 CAPL simulators send meaningful `0x101`/`0x201`/`0x301`/`0x401`/`0x501` messages, ECU6 code sends `0x601`, OTA `0x650`/`0x651` paths exist, and `CANOE/ECU6_Panel_Bridge.can` now contains minimal bridge logic. Full six-ECU CANoe/bench execution trace or exported report is still pending. |
| REQ-INT-007 | Complete | OTA success flow now unlocks security, receives data, verifies checksum, activates update, reaches SUCCESS, and updates `ActiveSWVersion`. |
| REQ-INT-008 | Complete | Wrong OTA payload checksum now produces NACK detail `0x17`, OTA_FAILED, and CAPL checks `ActiveSWVersion` remains unchanged. |
| REQ-INT-009 | Complete | Invalid ID and wrong checksum gateway-security behavior are implemented and covered by CAPL assets. |

## Next Implementation Targets

1. Collect S32DS flash/runtime evidence and CANoe execution reports.
2. Run `cmm/interrupt_evidence.cmm` in Trace32 and capture the required interrupt/GatewayStatus screenshots.
3. Run `cmm/scheduler_evidence.cmm` in Trace32 and capture 10 ms / 100 ms / 1000 ms counter growth.
4. Export readable CANoe reports for gateway, OTA/security, and full ECU1~ECU6 integration runs.
