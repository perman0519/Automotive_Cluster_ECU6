# ECU6 Gateway/OTA/Security Test Report

Source requirement document: `C:\Users\JunsangS\Downloads\MPC5748G_Embedded_Training_Requirements_EN.docx`

Build status: not executed by Codex. User-side S32DS `Debug_FLASH/Automotive_Cluster.elf` and `Debug_FLASH/Automotive_Cluster.map` artifacts were observed with 2026-06-02 09:41 KST timestamps; flash/hardware runtime was not verified by Codex.

Execution status: this report records available test assets, executed host-side tests, and the expected evidence to collect in CANoe/Trace32. CANoe `.vtestreport` artifacts exist for the gateway and OTA/security test sets, but their current binary report format did not expose readable pass/fail verdicts during the monitor run. The CANoe configuration references ECU1..ECU5, ECU6 gateway/OTA test CAPL, and the ECU6 panel bridge CAPL. Trace32 screenshots and reviewed/exported CANoe verdicts are still pending unless noted by the user.

Latest monitor note: On 2026-06-04, host-side unit tests were added under `Tests/host/` and executed with bundled Python; 9/9 tests passed and the result is recorded in `Documentation/ecu6_gateway/evidence/host_unit_tests_2026-06-04.txt`. Scheduler runtime counters were added in `Sources/services/ms_scheduler.c`, `cmm/scheduler_evidence.cmm` was added for Trace32 capture, and `CANOE/ECU6_Panel_Bridge.can` was replaced with a minimal sysvar-to-OTA bridge. A new S32DS build is required before the scheduler counter symbols are present in `Debug_FLASH/Automotive_Cluster.elf`. Runtime S32DS/CANoe/Trace32 evidence remains pending until manually executed/exported. Text search of `Configuration1.cfg` shows the active DBC reference as `CANOE\ECU5_UDS 1.dbc`; the intended ECU6 gateway/OTA DBC reference should be reviewed in CANoe before final communication/integration closure.

## 1. Test Assets

| Asset | Purpose |
| --- | --- |
| `CANOE/ECU1.can` through `CANOE/ECU5.can` | Simulated primary status messages with alive counter and checksum. `CANOE/ECU4.can` now provides a reviewed `0x401` ClusterStatus simulator with 100 ms cycle, alive counter, and XOR checksum. |
| `CANOE/ECU6_Gateway_OTA.dbc` | Primary DBC for monitored ECU1..ECU5 status frames, ECU6 `0x601`, OTA request `0x650`, OTA response `0x651`, and UDS diagnostic frames. |
| `Configuration1.cfg`, `Configuration1.stcfg` | CANoe configuration artifacts now referencing `CANOE/ECU6_Gateway_OTA.dbc`. |
| `CANOE/Panel1.xvp` | CANoe panel asset referenced by `Configuration1.cfg`; execution/use evidence is pending. |
| `CANOE/ECU6_Panel_Bridge.can` | CANoe panel bridge CAPL source referenced by `Configuration1.cfg`; now sends OTA `0x650` commands from ECU6 panel sysvars and updates panel sysvars from `0x601`/`0x651`. CANoe compile/execution evidence is pending. |
| `CANOE/ECU5_UDS.dbc`, `CANOE/ECU6_Gateway_OTA_ClusterStatus_modified.dbc`, `CANOE/ECU6_UDS_FINAL.dbc` | Additional DBC copies documenting ClusterStatus, DiagnosticStatus, GatewayStatus, OTA, and UDS frames; contents match each other. |
| `CANOE/ECU6_Gateway_OTA_merged.dbc`, `CANOE/6_ECUs_dbc_0206.dbc`, `CANOE/6_ECUs_dbc_0206.ini` | Additional six-ECU DBC/database artifacts observed after the 16:04 KST pass; active CANoe config reference still needs review before using them as primary evidence. |
| `CANOE/ECUDBC.dbc`, `CANOE/ECUDBC.ini` | CANoe database editor artifacts; `ECU6_Gateway_OTA.dbc` is the reviewed primary DBC. |
| `CANOE/ECU6_Gateway_Test.can` | Gateway/CANoe test cases for ECU6. |
| `CANOE/ECU6_Gateway_Test_report.vtestreport` | Gateway CANoe report artifact updated at 2026-06-02 16:40 KST; readable verdicts still need review/export. |
| `CANOE/ECU6_OTA_State_Test.can` | OTA state and seed/key security CAPL tests for ECU6. |
| `CANOE/ECU6_OTA_State_Test_report.vtestreport` | OTA/security CANoe report artifact updated at 2026-06-02 17:07 KST; readable verdicts still need review/export. |
| `cmm/init_debug.cmm` | Trace32 reset/load/run entry script. |
| `cmm/test_debug.cmm` | General Trace32 test/debug script. |
| `cmm/gateway_debug.cmm` | Gateway-focused Trace32 breakpoints and watches. |
| `cmm/ota_debug.cmm` | OTA transport debug anchor. |
| `cmm/security_debug.cmm` | Security debug script for seed/key unlock, OTA_START security gate, and lockout variables. |
| `cmm/startup_dashboard.cmm`, `cmm/automotive_toolbar.cmm` | Trace32 dashboard and toolbar helpers for the project. |
| `cmm/interrupt_evidence.cmm` | Trace32 screenshot workflow for tick ISR, CAN ISR, gateway RX handling, and GatewayStatus TX evidence. |
| `cmm/scheduler_evidence.cmm` | Trace32 screenshot workflow for `gComScheduler10msCount`, `gComScheduler100msCount`, `gComScheduler1000msCount`, and `gComSchedulerLastCycle10ms`. |
| `Tests/host/test_ecu6_gateway_logic.py` | Host-side logic tests for checksum, unknown ID, alive freeze, timeout, seed/key, OTA checksum, and security lockout. |
| `Documentation/ecu6_gateway/evidence/host_unit_tests_2026-06-04.txt` | Executed host test evidence; 9/9 tests passed with bundled Python. |

## 2. Current CAPL Test Coverage

| Test case | Requirement coverage | Expected result | Current status |
| --- | --- | --- | --- |
| `TC_ECU6_001_GatewayAllowValidMessage` | REQ-ECU6-001, REQ-ECU6-014, REQ-COM-003, REQ-COM-006, REQ-COM-007 | GatewayStatus reaches NORMAL after valid ECU1..ECU5 messages. | Available, execution evidence pending. |
| `TC_ECU6_002_GatewayBlockInvalidID` | REQ-ECU6-002 | Unknown CAN ID increments `BlockedMsgCnt`. | Available, execution evidence pending. |
| `TC_ECU6_003_GatewayBlockWrongChecksum` | REQ-ECU6-003 | Wrong checksum sets checksum error and increments blocked count. | Available, execution evidence pending. |
| `TC_ECU6_Transport_OTARequestResponse` | REQ-ECU6-006 | OTA request on `0x650` produces OTA response on `0x651`. | Available, execution evidence pending. |
| `TC_ECU6_005_OTAStateMachine` | REQ-ECU6-005, REQ-ECU6-007 | START/DATA/END/ACTIVATE/ABORT transitions are checked through OTA response and GatewayStatus. | Available, execution evidence pending. |
| `TC_ECU6_010_OTARejectWithoutSecurity` | REQ-ECU6-010 | START is rejected before security unlock. | Available in `ECU6_OTA_State_Test.can`, execution evidence pending. |
| `TC_ECU6_011_SecurityUnlockOK` | REQ-ECU6-011 | Correct seed/key sequence unlocks OTA. | Available in `ECU6_OTA_State_Test.can`, execution evidence pending. |
| `TC_ECU6_011_SecurityUnlockFail` | REQ-ECU6-011 | Wrong key returns NACK and increments security error counter. | Available in `ECU6_OTA_State_Test.can`, execution evidence pending. |
| `TC_ECU6_008_OTADataBuffer100Bytes` | REQ-ECU6-008 | A 100-byte OTA_DATA transfer is accepted after unlock and START, then reaches SUCCESS through the current simulated verification path. | Available in `ECU6_OTA_State_Test.can`, execution evidence pending. |
| `TC_ECU6_008_OTADataBufferOverflow` | REQ-ECU6-008 | Data beyond the 256-byte OTA buffer is rejected with NACK detail `0x16`. | Available in `ECU6_OTA_State_Test.can`, execution evidence pending. |
| `TC_ECU6_009_OTAChecksumOK` | REQ-ECU6-009, REQ-INT-007 | Correct OTA payload checksum reaches VERIFY_OK/READY_TO_ACTIVATE and activation SUCCESS. | Available in `ECU6_OTA_State_Test.can`, execution evidence pending. |
| `TC_ECU6_009_OTAChecksumFail` | REQ-ECU6-009, REQ-INT-008 | Wrong OTA payload checksum returns NACK detail `0x17`, OTA_FAILED, and leaves `ActiveSWVersion` unchanged. | Available in `ECU6_OTA_State_Test.can`, execution evidence pending. |
| `TC_ECU6_012_OTADataAuthFail` | REQ-ECU6-012 | Wrong OTA_DATA AuthByte returns NACK detail `0x18`, increments `SecurityErrorCounter`, and OTA_FAILED. | Available in `ECU6_OTA_State_Test.can`, execution evidence pending. |
| `TC_ECU6_013_SecurityLockoutAfterThreeFailures` | REQ-ECU6-013 | Three wrong keys trigger lockout, REQUEST_SEED is rejected with detail `0x23`, then accepted again after timeout. | Available in `ECU6_OTA_State_Test.can`, execution evidence pending. |
| `TC_ECU6_015_ActiveSWVersionUpdateAfterActivation` | REQ-ECU6-015, REQ-INT-007 | Version remains unchanged through READY_TO_ACTIVATE, increments on verified ACTIVATE success, and does not increment on a repeated invalid ACTIVATE. | Available in `ECU6_OTA_State_Test.can`, execution evidence pending. |

## 2.1 Host-Side Unit Test Coverage

Command executed from the repository root:

```powershell
C:\Users\JunsangS\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe -m unittest discover -s Tests/host -p "test_*.py" -v
```

Result: 9 tests passed.

| Host test | Requirement coverage | Result |
| --- | --- | --- |
| `test_checksum_calculation_and_validation` | REQ-COM-007, REQ-ECU6-003 | Pass |
| `test_unknown_id_increments_blocked_counter` | REQ-ECU6-002 | Pass |
| `test_alive_counter_freeze_and_recovery` | REQ-ECU6-004 | Pass |
| `test_timeout_detection_and_recovery` | REQ-COM-005 | Pass |
| `test_seed_key_unlock_and_start_gate` | REQ-ECU6-010, REQ-ECU6-011 | Pass |
| `test_ota_checksum_success_and_activation` | REQ-ECU6-009, REQ-ECU6-015, REQ-INT-007 | Pass |
| `test_ota_checksum_failure_keeps_version` | REQ-ECU6-009, REQ-INT-008 | Pass |
| `test_security_lockout_and_recovery` | REQ-ECU6-013 | Pass |
| `test_source_constants_match_host_model` | REQ-COM-015 traceability | Pass |

## 3. Missing or Incomplete Tests

| Missing test | Related requirement |
| --- | --- |
| CANoe alive counter freeze injection/exported verdict | REQ-ECU6-004 |
| CANoe monitored ECU timeout injection/exported verdict | REQ-COM-005 |
| Trace32 scheduler counter screenshot/export | REQ-COM-004 |
| S32DS 10-minute runtime log/screenshot | REQ-COM-001 |
| Readable CANoe gateway/OTA/full integration report exports | REQ-COM-003, REQ-INT-001 |

## 4. Manual Verification Checklist

| Step | Evidence to capture |
| --- | --- |
| Build and flash from S32DS | Rebuild after the scheduler counter change, then capture flash/runtime screenshot or note. |
| Observe UART boot log | UART terminal screenshot showing `[time][ECU6][module][level] message`. |
| Run normal gateway CANoe test | CANoe report for `TC_ECU6_001_GatewayAllowValidMessage`. |
| Inject unknown ID | CANoe report showing `BlockedMsgCnt` increment. |
| Inject wrong checksum | CANoe report showing checksum error flag. |
| Freeze alive counter | CANoe trace showing alive error flag after N repeats. |
| Stop one monitored message | CANoe trace showing timeout/alive error and later recovery. |
| Run Trace32 gateway script | Trace32 screenshot with breakpoints and watched variables. |
| Run Trace32 interrupt evidence script | Screenshots of `vPortTickISR`, CAN mailbox ISR, `COM_GatewayHandleRxMessage`, and `COM_GatewayBuildStatusMessage`/`CAN_SendMessage`. |
| Run Trace32 scheduler evidence script | Screenshots showing `gComScheduler10msCount`, `gComScheduler100msCount`, and `gComScheduler1000msCount` increasing at the expected ratios. |
| Run full six-ECU CANoe integration | Exported trace/report showing `0x101`, `0x201`, `0x301`, `0x401`, `0x501`, `0x601`, `0x650`, and `0x651`. |

## 5. Result Summary

No build, hardware flash/run, CANoe measurement, or Trace32 target session was run by Codex during this monitoring pass. User-side S32DS `Debug_FLASH` build artifacts and updated CANoe `.vtestreport` artifacts were observed, but flash/runtime execution and readable CANoe verdict evidence remain pending.

Host-side unit tests were added and executed by Codex with the bundled Python runtime. The host tests passed 9/9 and close REQ-COM-015.

Code review shows gateway monitoring, checksum, alive counter, timeout supervision, 0x601 periodic status, 0x650/0x651 transport, OTA command-state transitions, seed/key security unlock, bounded OTA payload buffering, OTA payload checksum verification, OTA_DATA AuthByte validation, failed-attempt lockout, scheduler counters, and ActiveSWVersion update after verified activation are present. Runtime execution evidence is still pending for REQ-COM-001, REQ-COM-003, REQ-COM-004, and REQ-INT-001.
