# ECU6 Gateway 발표 준비 가이드

분석 기준일: 2026-06-04 KST

분석 위치: `C:\Users\JunsangS\workspaceS32DS.Power.2.1\Automotive_Cluster`

참고한 원문/문서:

- `MPC5748G_Embedded_Training_Requirements_EN.docx`
- `MPC5748G_Embedded_Training_Requirements_EN.pdf`
- `Documentation/ecu6_gateway/requirements_status.md`
- `Documentation/ecu6_gateway/test_report.md`
- `Documentation/ecu6_gateway/design.md`

주의해서 말할 점:

- 코드, DBC, CAPL, CMM asset은 확인되었다.
- S32DS 빌드 산출물은 `Debug_FLASH/Automotive_Cluster.elf`, `.map`이 관찰되었다는 문서 기록이 있다.
- Codex가 CANoe, Trace32, 실제 보드 실행을 수행한 것은 아니다.
- `.vtestreport` 파일은 존재하지만 텍스트 검색으로 PASS/FAIL verdict를 읽을 수 없었다. 발표에서는 “CANoe 실행 리포트 파일은 있으나 readable verdict export 또는 screenshot이 추가로 필요하다”고 말해야 한다.

## 1. 프로젝트 전체 구조

이 프로젝트는 S32DS Power 기반 MPC5748G FreeRTOS 프로젝트이며, 현재 소스와 문서의 중심 역할은 `ECU6 Gateway + OTA + Security Node`이다.

주요 폴더 역할:

| 폴더 | 역할 | 발표 표현 |
| --- | --- | --- |
| `Sources/` | ECU6 firmware 소스. 초기화, FreeRTOS task, CAN driver wrapper, gateway/OTA/security service가 있다. | “실제 ECU6 동작 로직이 들어 있는 구현 산출물입니다.” |
| `Sources/drivers/` | CAN, GPIO, UART용 하위 driver wrapper. Processor Expert/PAL API를 직접 다룬다. | “하드웨어 접근을 service와 분리한 계층입니다.” |
| `Sources/services/` | gateway 판단, OTA 상태기계, logging, scheduler, system init. | “요구사항의 대부분은 service 계층에서 구현됩니다.” |
| `CANOE/` | DBC, CAPL simulator/test, panel bridge, `.vtestreport`. | “CANoe에서 신호를 해석하고 정상/오류/OTA 시나리오를 주입하는 산출물입니다.” |
| `cmm/` | Trace32 CMM script. init, gateway, OTA, security, interrupt evidence, dashboard. | “실제 보드에서 breakpoint와 변수 watch를 반복 가능하게 여는 디버깅 증적 수집 도구입니다.” |
| `Documentation/ecu6_gateway/` | 설계, 테스트 리포트, 요구사항 상태. | “구현과 테스트 asset을 요구사항 ID에 연결한 문서 산출물입니다.” |

Software 구조:

- `Sources/main.c`: `system_init()`, `can_init()` 후 FreeRTOS task 생성.
- `Sources/drivers/can.c`: CAN mailbox/filter/Tx/Rx 처리. 허용 ID mailbox, OTA mailbox, catch-all mailbox를 설정한다.
- `Sources/services/com_gateway.c`: allowed ID 판단, unknown ID counting, checksum, alive, timeout, `GatewayStatus 0x601`, OTA/security state machine.
- `Sources/services/ms_scheduler.c`: 10 ms, 100 ms, 1000 ms scheduler counter.
- `Sources/services/system_init.c`: clock, pin, UART, CAN 초기화.
- `Sources/automotive.h`: task, driver, service public interface 선언.

CAN driver와 gateway service 분리:

- `can.c`는 “CAN frame을 어떻게 받고 보낼지”를 담당한다.
- `com_gateway.c`는 “이 CAN ID가 허용되는지, checksum/alive/timeout이 정상인지, OTA/security 상태를 어떻게 바꿀지”를 담당한다.
- 발표 핵심 문장: “driver는 mailbox와 filter를 다루고, service는 요구사항 정책을 다룹니다. 그래서 CAN peripheral 설정과 gateway 판단 로직이 섞이지 않습니다.”

FreeRTOS task 구조:

| Task | 생성 위치 | 역할 |
| --- | --- | --- |
| `vInitLogTask` | `Sources/main.c` | boot/init log 출력 후 삭제 |
| `vLEDTask` | `Sources/main.c` | heartbeat LED toggle |
| `vCANTask` | `Sources/main.c`, 구현은 `Sources/drivers/can.c` | CAN RX polling, OTA request 처리, timeout check, `0x601` 100 ms 송신 |
| `vComSchedulerTask` | `Sources/main.c`, 구현은 `Sources/services/ms_scheduler.c` | 10 ms 기준 scheduler counter와 100 ms/1000 ms hook |

ECU6 CAN ID:

| CAN ID | 방향 | 의미 |
| --- | --- | --- |
| `0x101` | ECU1 -> ECU6 | `BodyStatus` monitoring |
| `0x201` | ECU2 -> ECU6 | `PowertrainStatus` monitoring |
| `0x301` | ECU3 -> ECU6 | `SensorStatus` monitoring |
| `0x401` | ECU4 -> ECU6 | `ClusterStatus` monitoring |
| `0x501` | ECU5 -> ECU6 | `DiagnosticStatus` monitoring |
| `0x601` | ECU6 -> CAN bus/CANoe | `GatewayStatus`, 100 ms periodic |
| `0x650` | CANoe -> ECU6 | `OTA_Request` |
| `0x651` | ECU6 -> CANoe | `OTA_Response` |

## 2. Sources 분석

| 파일 | 파일 역할 | 주요 함수 | 관련 요구사항 ID | 발표 핵심 문장 | 코드 확인 포인트 |
| --- | --- | --- | --- | --- | --- |
| `Sources/main.c` | application entry. platform init 후 FreeRTOS task 생성 | `main()` | `REQ-COM-001`, `REQ-COM-004`, `REQ-COM-008` | “`main.c`는 로직을 직접 구현하지 않고 초기화와 task orchestration만 담당합니다.” | `system_init()`, `can_init()`, `xTaskCreate(vCANTask)`, `xTaskCreate(vComSchedulerTask)` |
| `Sources/drivers/can.c` | CAN PAL wrapper와 mailbox/filter/Tx/Rx 처리 | `CAN_ConfigGatewayMailboxes()`, `CAN_ProcessOtaRxMessage()`, `CAN_ProcessGatewayAnyRxMessage()`, `vCANTask()` | `REQ-COM-003`, `REQ-ECU6-001`, `REQ-ECU6-002`, `REQ-ECU6-006`, `REQ-ECU6-014` | “CAN driver는 허용 ID mailbox, OTA mailbox, catch-all mailbox를 열어 service에 frame을 전달합니다.” | `0x101..0x501`, `0x650`, catch-all `CAN_RX_MAILBOX_ANY`, `COM_GatewayBuildStatusMessage()` 후 `CAN_SendMessage()` |
| `Sources/services/com_gateway.c` | ECU6 핵심 service. gateway, OTA, security 구현 | `COM_GatewayInit()`, `COM_GatewayHandleRxMessage()`, `COM_GatewayCheckTimeouts()`, `COM_GatewayBuildStatusMessage()`, `OTA_HandleRequestMessage()` | `REQ-COM-005..007`, `REQ-ECU6-001..015`, `REQ-INT-007..009` | “ECU6 요구사항의 대부분은 `com_gateway.c`에서 상태 변수와 state machine으로 구현됩니다.” | `rxMonitors[]`, `blockedMsgCnt`, `COM_VerifyChecksum()`, `COM_UpdateAliveMonitor()`, `OTA_ApplyCommandState()`, seed/key, lockout |
| `Sources/services/ms_scheduler.c` | 10 ms scheduler와 100 ms/1000 ms counter | `vComSchedulerTask()`, `COM_Task10ms()`, `COM_Task100ms()`, `COM_Task1000ms()` | `REQ-COM-004` | “main loop에 blocking delay를 두는 대신 FreeRTOS delay-until 기반 scheduler counter를 제공합니다.” | `vTaskDelayUntil(..., pdMS_TO_TICKS(10U))`, `gComScheduler*Count` |
| `Sources/services/system_init.c` | clock, pin, UART, CAN 초기화 | `system_init()`, `can_init()` | `REQ-COM-001` | “board bring-up은 clock/pin/UART/CAN 순서로 수행되고 CAN init 성공 시 LED4를 clear합니다.” | `CLOCK_SYS_Init`, `PINS_DRV_Init`, `UART_Init`, `CAN_Init` |
| `Sources/automotive.h` | common include와 public prototype | declarations | `REQ-COM-008`, `REQ-COM-014` | “각 module이 공유하는 public interface가 header에 모여 있어 계층 간 연결점이 보입니다.” | task prototype, CAN API, service API |

## 3. CANOE 분석

DBC 역할:

- DBC는 CAN ID와 payload byte/bit를 signal 이름으로 해석하는 “CAN 계약서”이다.
- `CANOE/ECU6_Gateway_OTA.dbc`에는 `BodyStatus`, `PowertrainStatus`, `SensorStatus`, `ClusterStatus`, `DiagnosticStatus`, `GatewayStatus`, `OTA_Request`, `OTA_Response`, UDS frame이 정의되어 있다.
- `GatewayStatus 0x601`의 bit layout은 `COM_GatewayBuildStatusMessage()`와 맞는다.

CANoe 산출물별 정리:

| 산출물 이름 | 목적 | 증명 요구사항 | 발표 설명 | 추가 실행 증적 필요 |
| --- | --- | --- | --- | --- |
| `CANOE/ECU6_Gateway_OTA.dbc` | ECU1-ECU6 status, OTA, UDS frame 정의 | `REQ-COM-003`, `REQ-COM-006`, `REQ-COM-007`, `REQ-ECU6-014` | “CANoe가 `0x601`을 GatewayState, OtaState, BlockedMsgCnt 등으로 decode할 수 있게 합니다.” | DBC 자체는 확인됨. CANoe trace screenshot 필요 |
| `CANOE/ECU1.can` | `0x101 BodyStatus` 100 ms simulator | `REQ-ECU6-001`, `REQ-COM-003` | “ECU1이 없을 때도 CANoe가 BodyStatus를 주기적으로 만들어 ECU6 monitoring을 시험합니다.” | 실행 trace 필요 |
| `CANOE/ECU2.can` | `0x201 PowertrainStatus` 50 ms simulator | `REQ-ECU6-001` | “PowertrainStatus의 alive/checksum 포함 frame을 만듭니다.” | 실행 trace 필요 |
| `CANOE/ECU3.can` | `0x301 SensorStatus` 100 ms simulator | `REQ-ECU6-001`, `REQ-ECU6-003` | “정상 SensorStatus와 wrong checksum injection의 기준 frame입니다.” | 실행 trace 필요 |
| `CANOE/ECU4.can` | `0x401 ClusterStatus` 100 ms simulator | `REQ-ECU6-001` | “ECU4 ClusterStatus도 ECU6 monitoring 대상임을 보여줍니다.” | 실행 trace 필요 |
| `CANOE/ECU5.can` | `0x501 DiagnosticStatus` 100 ms simulator | `REQ-ECU6-001` | “DiagnosticStatus를 통해 ECU5까지 five-node monitoring이 가능합니다.” | 실행 trace 필요 |
| `CANOE/ECU6_Gateway_Test.can` | gateway 정상/unknown ID/wrong checksum/OTA transport 기본 CAPL test | `REQ-ECU6-001`, `REQ-ECU6-002`, `REQ-ECU6-003`, `REQ-ECU6-006`, `REQ-ECU6-014`, `REQ-INT-009` | “`0x601`을 감시하고, invalid ID `0x123`과 bad checksum `0x301`을 주입합니다.” | `.vtestreport` readable verdict 또는 screenshot 필요 |
| `CANOE/ECU6_OTA_State_Test.can` | OTA/security state machine CAPL test | `REQ-ECU6-005..013`, `REQ-ECU6-015`, `REQ-INT-007`, `REQ-INT-008` | “REQUEST_SEED/SEND_KEY, START/DATA/END/ACTIVATE, overflow, checksum fail, lockout까지 검증하도록 구성되어 있습니다.” | `.vtestreport` readable verdict 또는 screenshot 필요 |
| `CANOE/ECU6_Panel_Bridge.can` | CANoe panel sysvar를 OTA request `0x650`으로 변환 | `REQ-ECU6-006`, `REQ-ECU6-010`, `REQ-ECU6-011`, `REQ-ECU6-015` | “패널 버튼을 누르면 CAPL이 seed/key/OTA command frame을 만들어 보냅니다.” | 패널 조작 screenshot 필요 |
| `CANOE/Panel1.xvp` | CANoe panel asset | ECU6 panel expectation | “시연자가 seed/key, OTA start/end/activate 등을 조작하는 UI 산출물입니다.” | 실제 panel screenshot 필요 |
| `CANOE/ECU6_Gateway_Test_report.vtestreport` | Gateway test report artifact | `REQ-COM-011`, gateway 관련 요구사항 | “리포트 파일은 있지만 현재 텍스트로 PASS/FAIL을 읽지 못했습니다.” | exported readable report 필요 |
| `CANOE/ECU6_OTA_State_Test_report.vtestreport` | OTA/security test report artifact | `REQ-COM-011`, OTA/security 관련 요구사항 | “OTA/security 테스트 실행 산출물 후보입니다.” | exported readable report 필요 |
| `Configuration1.cfg` / `Configuration1.stcfg` | CANoe configuration | `REQ-COM-003`, `REQ-COM-011`, `REQ-INT-001` | “DBC, CAPL node, panel, report file을 한 CANoe configuration에 묶는 실행 환경입니다.” | 실제 CANoe open/run screenshot 필요 |

특정 CAN ID 설명:

- `GatewayStatus 0x601`: ECU6가 100 ms마다 보내는 상태 요약. `GatewayState`, `OtaState`, `SecurityUnlocked`, `SecurityLocked`, `AliveError`, `ChecksumError`, `BlockedMsgCnt`, `SecurityErrorCounter`, `ActiveSWVersion`, `GatewayRxMask`, `AliveCounter`, `Checksum`을 담는다.
- `OTA_Request 0x650`: CANoe panel/test가 ECU6에 보내는 OTA/security command. command byte는 `START=0x01`, `DATA=0x02`, `END=0x03`, `ACTIVATE=0x04`, `ABORT=0x05`, `REQUEST_SEED=0x10`, `SEND_KEY=0x11`.
- `OTA_Response 0x651`: ECU6가 `0x650` 요청에 대해 ACK `0x79` 또는 NACK `0x7F`와 detail code를 반환한다.
- ECU1-ECU5 monitored ID: `0x101`, `0x201`, `0x301`, `0x401`, `0x501`.

## 4. cmm Trace32 분석

| CMM 파일 | 스크립트 목적 | 보는 변수 또는 breakpoint | 관련 요구사항 | 발표 문장 | 실제 보드 디버깅 증적 |
| --- | --- | --- | --- | --- | --- |
| `cmm/init_debug.cmm` | ELF load, reset, breakpoint at `main`, run-to-main | `Break.Set main`, `Register.view`, `SYStem.state` | `REQ-COM-010`, `REQ-COM-001` | “Trace32에서 target을 reset/load하고 `main`까지 진입하는 기본 bring-up script입니다.” | main breakpoint hit screenshot |
| `cmm/test_debug.cmm` | 일반 runtime review | `vCANTask`, `COM_GatewayHandleRxMessage`, `OTA_HandleRequestMessage`, scheduler counters | `REQ-COM-004`, `REQ-COM-010`, `REQ-COM-003` | “CAN task, gateway RX, OTA parser, scheduler counter를 한 화면에서 확인합니다.” | watched variables changing screenshot |
| `cmm/gateway_debug.cmm` | gateway 전용 debug | `COM_GatewayHandleRxMessage`, `COM_GatewayBuildStatusMessage`, `CAN_SendMessage`, `gatewayRxMask`, `blockedMsgCnt`, `rxMonitors` | `REQ-COM-005..007`, `REQ-ECU6-001..004`, `REQ-ECU6-014`, `REQ-ECU6-016` | “허용 ID 수신, unknown count, checksum/alive/timeout fault를 변수로 직접 확인합니다.” | invalid ID/checksum injection 후 watch screenshot |
| `cmm/ota_debug.cmm` | OTA transport/state/debug | `OTA_HandleRequestMessage`, `CAN_SendOtaResponse`, `otaState`, `otaDataLength`, `otaExpectedChecksum`, `otaVerifyOk`, `activeSwVersion` | `REQ-ECU6-005..009`, `REQ-ECU6-012`, `REQ-ECU6-015`, `REQ-ECU6-016` | “OTA command가 내부 상태와 buffer/checksum/version을 어떻게 바꾸는지 봅니다.” | OTA sequence별 variable screenshot |
| `cmm/security_debug.cmm` | seed/key와 lockout debug | `OTA_HandleRequestMessage`, `securityUnlocked`, `securityLocked`, `securityErrorCounter`, `securityFailureStreak`, `securityLockoutActive`, `securitySeed` | `REQ-ECU6-010`, `REQ-ECU6-011`, `REQ-ECU6-013`, `REQ-ECU6-016` | “START 전 보안 잠금, 올바른 key unlock, 3회 실패 lockout을 Trace32에서 확인합니다.” | wrong key 3회 후 lockout screenshot |
| `cmm/interrupt_evidence.cmm` | interrupt evidence capture 절차 | `vPortTickISR`, `CAN0_ORed_00_03_MB_IRQHandler`, `COM_GatewayHandleRxMessage`, scheduler/gateway watch | `REQ-COM-001`, `REQ-COM-003`, `REQ-ECU6-014` | “FreeRTOS tick ISR와 CAN mailbox ISR이 실제 service 경로로 이어지는지 증명하기 위한 script입니다.” | ISR hit screenshot, CANoe `0x601` trace |
| `cmm/startup_dashboard.cmm` | Trace32 dashboard setup | gateway/OTA/security 변수 watch, register/break/frame/system windows | `REQ-COM-010`, `REQ-ECU6-016` | “mentor review 때 필요한 화면을 한 번에 여는 dashboard입니다.” | dashboard screenshot |
| `cmm/automotive_toolbar.cmm` | Trace32 toolbar button 추가 | init/test/gateway/OTA/security/interrupt script 실행 버튼 | `REQ-COM-010`, `REQ-ECU6-016` | “반복 실행할 CMM들을 toolbar 버튼으로 묶어 디버깅 절차를 표준화합니다.” | toolbar visible screenshot |

## 5. 요구사항별 산출물 매핑표

| Requirement ID | 요구사항 의미 | 코드 산출물 | CANoe 산출물 | Trace32/CMM 산출물 | 문서 산출물 | 상태 | 발표 시 설명 포인트 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `REQ-COM-001` | clock/GPIO/UART/CAN/timer/interrupt init, 10분 안정 실행 | `main.c`, `system_init.c`, `gpio.c`, `can.c` | CAN trace 필요 | `init_debug.cmm`, `interrupt_evidence.cmm` | `requirements_status.md` | Partial | 초기화 코드는 있으나 10분 run/flash/interrupt screenshot 필요 |
| `REQ-COM-002` | UART logging format | `logger.c` | 없음 | runtime watch 가능 | `test_report.md` | Complete | `[time][ECU6][module][level]` 형식 구현 |
| `REQ-COM-003` | periodic CAN Tx/Rx | `vCANTask`, `CAN_ConfigGatewayMailboxes` | DBC, CAPL, `Configuration1.cfg`, `.vtestreport` | `test_debug.cmm`, `interrupt_evidence.cmm` | `requirements_status.md` | Partial | Tx/Rx asset은 있으나 readable CANoe verdict 필요 |
| `REQ-COM-004` | 10/100/1000 ms scheduler | `ms_scheduler.c` | 없음 | `test_debug.cmm`, `scheduler_evidence.cmm` | `requirements_status.md` | Partial | counters 존재, 실제 breakpoint 증적 필요 |
| `REQ-COM-005` | timeout monitoring | `COM_GatewayCheckTimeouts()` | timeout injection test는 미흡 | `gateway_debug.cmm` | `test_report.md` | Complete | 코드상 timeout flag/log/GatewayStatus 반영 |
| `REQ-COM-006` | 4-bit alive counter | `COM_GatewayBuildStatusMessage()`, monitored frame parser | DBC alive signals, CAPL alive check | `gateway_debug.cmm` | `design.md` | Complete | `data[6] & 0x0F`, 0..15 wrap |
| `REQ-COM-007` | checksum | `COM_CalculateChecksum()`, `COM_VerifyChecksum()` | CAPL `calcChecksum()` | `gateway_debug.cmm` | `design.md` | Complete | CAN ID LSB XOR data[0..6] |
| `REQ-COM-008` | driver/service/application separation | `Sources/drivers`, `Sources/services`, `main.c` | 없음 | 없음 | `design.md` | Complete | driver는 CAN access, service는 policy |
| `REQ-COM-009` | state machine | gateway/OTA/security state in `com_gateway.c` | OTA CAPL tests | `ota_debug.cmm`, `security_debug.cmm` | `design.md` | Complete | gateway/OTA/security 상태가 testable |
| `REQ-COM-010` | `init_debug.cmm`, `test_debug.cmm` | 없음 | 없음 | `init_debug.cmm`, `test_debug.cmm` | `test_report.md` | Complete | CMM script asset 존재 |
| `REQ-COM-011` | 최소 3개 CAPL test | 없음 | `ECU6_Gateway_Test.can`, `ECU6_OTA_State_Test.can` | 없음 | `test_report.md` | Complete | testcase는 충분, 실행 verdict는 별도 필요 |
| `REQ-COM-012` | design/test documentation | 없음 | 없음 | 없음 | `design.md`, `test_report.md` | Complete | 문서 산출물 존재 |
| `REQ-COM-013` | fault recovery | checksum/timeout/alive recovery logic | 일부 CAPL asset | `gateway_debug.cmm` | `design.md` | Complete | valid frame 재수신 시 회복 path |
| `REQ-COM-014` | traceable names/comments | source comments, names | testcase names | CMM requirement comments | docs | Complete | 요구사항 ID 연결 가능 |
| `REQ-COM-015` | host-side unit tests | 찾지 못함 | 없음 | 없음 | `requirements_status.md` | Missing | optional P2, hardware-independent test 미존재 |
| `REQ-ECU6-001` | allowed ID monitoring | `rxMonitors[]`, exact mailbox filters | ECU1-ECU5 CAPL simulators | `gateway_debug.cmm` | `design.md` | Complete | `0x101..0x501`만 monitored |
| `REQ-ECU6-002` | unknown ID blocking/counting | `COM_FindRxMonitor()`, `blockedMsgCnt` | `TC_ECU6_002`, invalid `0x123` | `gateway_debug.cmm` | `test_report.md` | Complete | catch-all mailbox가 service로 보내고 counter 증가 |
| `REQ-ECU6-003` | checksum validation | `COM_VerifyChecksum()` | `TC_ECU6_003` bad `0x301` | `gateway_debug.cmm` | `design.md` | Complete | wrong checksum은 blocked count와 checksum error |
| `REQ-ECU6-004` | alive counter monitoring | `COM_UpdateAliveMonitor()` | alive freeze injection은 추가 필요 | `gateway_debug.cmm` | `test_report.md` | Complete | 코드 구현은 있음, freeze test screenshot 추가 권장 |
| `REQ-ECU6-005` | OTA states | `OTA_ApplyCommandState()` | OTA state tests | `ota_debug.cmm` | `design.md` | Complete | IDLE부터 FAILED까지 `0x601`로 관찰 |
| `REQ-ECU6-006` | `0x650`/`0x651` OTA transport | `CAN_ProcessOtaRxMessage()`, `OTA_HandleRequestMessage()` | OTA request/response CAPL | `ota_debug.cmm` | `design.md` | Complete | request에 대해 ACK/NACK response 생성 |
| `REQ-ECU6-007` | START/DATA/END/ACTIVATE/ABORT | `OTA_ApplyCommandState()` | full path/invalid sequence tests | `ota_debug.cmm` | `test_report.md` | Complete | 유효 sequence만 성공 |
| `REQ-ECU6-008` | 256-byte OTA buffer | `otaDataBuffer[256]`, `OTA_StoreDataPayload()` | 100-byte/overflow tests | `ota_debug.cmm` | `test_report.md` | Complete | overflow는 NACK detail `0x16` |
| `REQ-ECU6-009` | OTA checksum before activation | `OTA_VerifyDataChecksum()` | checksum OK/fail tests | `ota_debug.cmm` | `design.md` | Complete | Byte2 expected checksum, XOR over buffer |
| `REQ-ECU6-010` | unlock before `OTA_START` | security gate in `OTA_ApplyCommandState()` | `TC_ECU6_010` | `security_debug.cmm` | `test_report.md` | Complete | locked 상태에서 START는 NACK `0x20` |
| `REQ-ECU6-011` | seed/key security | `Security_GenerateSeed()`, `Security_CalculateKey()` | unlock OK/fail tests | `security_debug.cmm` | `design.md` | Complete | key는 `seed XOR 0xA5` |
| `REQ-ECU6-012` | OTA_DATA AuthByte | `OTA_VerifyDataAuth()` | auth fail test | `ota_debug.cmm` | `design.md` | Complete | `seed XOR sequence XOR length XOR 0xC3` |
| `REQ-ECU6-013` | 3회 실패 lockout | `Security_RecordFailure()` | lockout test | `security_debug.cmm` | `design.md` | Complete | 3회 실패 후 3000 ms lockout |
| `REQ-ECU6-014` | `0x601` 100 ms periodic | `vCANTask()`, `COM_GatewayBuildStatusMessage()` | DBC, CAPL `on message 0x601` | `gateway_debug.cmm`, `interrupt_evidence.cmm` | `design.md` | Complete | 100 ms마다 GatewayStatus 송신 |
| `REQ-ECU6-015` | `ActiveSWVersion` update | `OTA_AdvanceActiveSwVersion()` | ActiveSWVersion test | `ota_debug.cmm` | `test_report.md` | Complete | verified ACTIVATE에서만 version 증가 |
| `REQ-ECU6-016` | gateway/OTA/security CMM | 없음 | 없음 | gateway/OTA/security/startup/interrupt CMM | `test_report.md` | Complete | CMM script set 존재 |
| `REQ-INT-001` | six-board same CAN network | ECU6 side implemented | simulators/config/DBC | dashboard/interrupt CMM | `requirements_status.md` | Partial | full six-board runtime trace 필요 |
| `REQ-INT-007` | OTA success scenario | OTA success path | checksum OK/version tests | `ota_debug.cmm` | `test_report.md` | Complete | asset 기준 complete, runtime verdict 필요 |
| `REQ-INT-008` | OTA fail scenario | checksum mismatch path | checksum fail test | `ota_debug.cmm` | `test_report.md` | Complete | wrong checksum에서 version 유지 |
| `REQ-INT-009` | gateway security scenario | unknown/checksum reject path | invalid ID/wrong checksum CAPL | `gateway_debug.cmm` | `test_report.md` | Complete | invalid traffic이 `0x601`에 반영 |

## 6. ECU6 Gateway 핵심 요구사항 쉬운 설명

- allowed CAN ID monitoring: ECU6는 `rxMonitors[]`에 등록된 `0x101`, `0x201`, `0x301`, `0x401`, `0x501`만 정상 monitoring 대상으로 본다. 정상 frame이 오면 `gatewayRxMask` bit가 채워진다.
- unknown CAN ID blocking/counting: 등록되지 않은 ID는 data를 사용하지 않고 `blockedMsgCnt`만 증가시킨다. CAN driver의 catch-all mailbox가 unknown ID를 service까지 전달해 count 가능하게 한다.
- checksum validation: 모든 primary status frame은 `CAN_ID_LSB XOR data[0..6]` 값이 `data[7]`에 있어야 한다. 틀리면 checksum error와 blocked count가 증가한다.
- alive counter monitoring: `data[6] & 0x0F`가 alive counter다. 같은 값이 `COM_ALIVE_FREEZE_LIMIT`만큼 반복되면 alive freeze fault로 본다.
- timeout monitoring: 각 monitored ID는 `COM_RX_TIMEOUT_MS=300 ms` 안에 들어와야 한다. 끊기면 timeout/alive error가 `GatewayStatus`에 반영된다.
- `GatewayStatus 0x601` periodic transmission: `vCANTask()`가 100 ms마다 `COM_GatewayBuildStatusMessage()`로 payload를 만들고 `CAN_SendMessage()`로 송신한다.
- OTA request/response `0x650`/`0x651`: CANoe가 `0x650`으로 command를 보내면 ECU6가 `0x651`로 ACK/NACK, detail, state, version, response counter를 보낸다.
- OTA state machine: `IDLE -> START -> RECEIVING -> VERIFYING -> READY_TO_ACTIVATE -> SUCCESS`가 정상 흐름이고, invalid sequence, abort, checksum/auth/security 실패는 `FAILED`로 간다.
- seed/key security unlock: `REQUEST_SEED 0x10`으로 seed를 받고, `SEND_KEY 0x11`에서 `seed XOR 0xA5`를 보내야 unlock된다.
- OTA data buffer: `OTA_CMD_DATA`에서 Byte2는 payload length, Byte3는 AuthByte, Byte4-Byte7은 payload다. 총 256 bytes까지 저장하고 넘으면 NACK `0x16`.
- OTA checksum verification: `OTA_CMD_END`의 Byte2가 expected checksum이다. ECU6는 저장된 OTA data를 XOR해서 비교한다.
- security lockout: wrong key 또는 wrong AuthByte가 3회 누적되면 3000 ms lockout이 걸리고, 이 동안 known command는 NACK `0x23`으로 거절된다.
- `ActiveSWVersion` update: checksum 검증 후 `READY_TO_ACTIVATE` 상태에서 `ACTIVATE`가 성공해야만 version이 증가한다. 실패하거나 중복 activate하면 증가하지 않는다.

## 7. 5-7분 발표 스크립트

안녕하세요. 제가 발표할 프로젝트는 MPC5748G와 S32DS Power, FreeRTOS, CAN, CANoe, Trace32를 사용한 Automotive Cluster training project입니다. 전체 요구사항은 6개 ECU가 같은 CAN network에서 동작하는 구조이고, 이 프로젝트는 그중 `ECU6 Gateway + OTA + Security Node`에 초점을 맞추고 있습니다.

ECU6의 역할은 크게 세 가지입니다. 첫째, ECU1부터 ECU5까지의 상태 메시지 `0x101`, `0x201`, `0x301`, `0x401`, `0x501`을 monitoring합니다. 둘째, gateway 상태를 `GatewayStatus 0x601`로 100 ms마다 전송합니다. 셋째, CANoe에서 들어오는 `OTA_Request 0x650`을 처리하고 `OTA_Response 0x651`로 ACK 또는 NACK을 반환하면서 OTA와 security state machine을 제공합니다.

소프트웨어 구조는 driver와 service가 분리되어 있습니다. `Sources/main.c`는 `system_init()`과 `can_init()`을 수행하고, FreeRTOS task를 생성합니다. `Sources/drivers/can.c`는 CAN mailbox, filter, Tx/Rx wrapper를 담당합니다. 반면 `Sources/services/com_gateway.c`는 allowed ID 판단, unknown ID counting, checksum validation, alive counter monitoring, timeout monitoring, OTA state machine, seed/key security, lockout, ActiveSWVersion update 같은 실제 요구사항 로직을 담당합니다. 그래서 하드웨어 접근과 gateway 정책이 섞이지 않는 구조입니다.

CAN 통신 구조를 보면 ECU6는 다섯 개 primary status message를 monitoring합니다. `0x101`은 BodyStatus, `0x201`은 PowertrainStatus, `0x301`은 SensorStatus, `0x401`은 ClusterStatus, `0x501`은 DiagnosticStatus입니다. ECU6가 보내는 `0x601 GatewayStatus`에는 GatewayState, OtaState, SecurityUnlocked, SecurityLocked, AliveError, ChecksumError, BlockedMsgCnt, SecurityErrorCounter, ActiveSWVersion, GatewayRxMask, AliveCounter, Checksum이 들어갑니다. OTA는 CANoe가 `0x650`으로 요청하고 ECU6가 `0x651`로 응답하는 event 기반 구조입니다.

요구사항 구현을 예로 들면, allowed ID monitoring은 `rxMonitors[]`에 등록된 ID만 정상 처리하는 방식입니다. unknown ID가 들어오면 사용하지 않고 `blockedMsgCnt`만 증가시킵니다. checksum은 CAN ID LSB와 data[0]부터 data[6]까지 XOR한 값을 data[7]과 비교합니다. alive counter는 data[6]의 하위 4비트이고, 같은 값이 반복되면 alive freeze로 판단합니다. timeout은 300 ms 기준으로 monitored message가 오지 않으면 error flag를 세웁니다. 이 결과들은 모두 `0x601 GatewayStatus`에 반영됩니다.

OTA와 security는 state machine으로 설명할 수 있습니다. 처음에는 security locked 상태이므로 `OTA_START`는 거절됩니다. CANoe가 `REQUEST_SEED 0x10`을 보내면 ECU6가 seed를 `0x651`로 반환하고, `SEND_KEY 0x11`에서 `seed XOR 0xA5`가 맞으면 unlock됩니다. 이후 START, DATA, END, ACTIVATE 순서로 진행합니다. DATA는 최대 256 bytes buffer에 저장되며 AuthByte도 검사합니다. END에서 저장된 payload checksum을 검증하고, 검증이 성공하면 READY_TO_ACTIVATE가 됩니다. 이 상태에서 ACTIVATE가 성공해야만 `ActiveSWVersion`이 증가합니다. wrong key나 wrong AuthByte가 3회 발생하면 3000 ms lockout이 걸립니다.

CANoe 산출물은 DBC, CAPL test, panel bridge, report artifact로 나뉩니다. DBC는 `0x601`, `0x650`, `0x651`을 signal 단위로 decode하는 기준입니다. `ECU1.can`부터 `ECU5.can`은 실제 보드가 없어도 CANoe에서 periodic status frame을 만들어 ECU6 monitoring을 시험하게 해 줍니다. `ECU6_Gateway_Test.can`은 정상 gateway, unknown ID, wrong checksum, OTA transport 기본을 검증하고, `ECU6_OTA_State_Test.can`은 seed/key, OTA buffer, checksum, AuthByte, lockout, ActiveSWVersion까지 검증합니다. 다만 `.vtestreport` 파일은 존재하지만 PASS/FAIL verdict를 텍스트로 확인하지 못했기 때문에, 발표에서는 실행 결과 screenshot 또는 readable export가 추가로 필요하다고 구분해서 말하겠습니다.

Trace32 산출물은 실제 보드 디버깅 증적을 얻기 위한 CMM script입니다. `init_debug.cmm`은 reset/load/run-to-main을 수행하고, `gateway_debug.cmm`은 `gatewayRxMask`, `blockedMsgCnt`, `rxMonitors`를 보며 gateway 요구사항을 확인합니다. `ota_debug.cmm`은 `otaState`, `otaDataLength`, checksum, AuthByte, `activeSwVersion`을 watch합니다. `security_debug.cmm`은 seed/key, unlock, error counter, lockout을 확인합니다. `interrupt_evidence.cmm`은 FreeRTOS tick ISR와 CAN mailbox ISR이 실제로 동작하는지 screenshot을 남기기 위한 절차입니다.

현재 결론은 코드와 테스트 asset 수준에서는 ECU6 gateway/OTA/security 요구사항 대부분이 구현되어 있다는 것입니다. 하지만 실제 완료 증적으로 말하려면 CANoe readable PASS/FAIL report, CAN trace screenshot, Trace32 breakpoint/watch screenshot, S32DS flash/runtime evidence가 추가로 필요합니다. 따라서 발표에서는 “구현과 검증 asset은 준비되어 있고, runtime evidence는 별도 수집 대상”이라고 정확히 구분하겠습니다.

## 8. 예상 질문과 답변

| 질문 | 답변 |
| --- | --- |
| 왜 CAN driver와 gateway service를 분리했는가? | CAN driver는 mailbox, filter, Tx/Rx처럼 hardware/PAL 의존 코드를 담당하고, gateway service는 allowed ID, checksum, alive, timeout, OTA/security 정책을 담당합니다. 이렇게 분리하면 hardware 설정 변경과 요구사항 로직 변경의 영향 범위가 줄어듭니다. |
| alive counter와 checksum은 왜 필요한가? | alive counter는 메시지가 계속 갱신되는지 확인하기 위한 것이고, checksum은 payload가 깨지거나 잘못 만들어졌는지 확인하기 위한 것입니다. ECU6는 둘 다 검사해서 통신 신뢰성을 판단합니다. |
| `0x601 GatewayStatus`는 어떤 정보를 담는가? | GatewayState, OtaState, SecurityUnlocked/Locked, AliveError, ChecksumError, BlockedMsgCnt, SecurityErrorCounter, ActiveSWVersion, GatewayRxMask, GatewayAliveCounter, Checksum을 담습니다. |
| CANoe 테스트는 무엇을 검증하는가? | ECU1-ECU5 simulator로 정상 frame을 만들고, ECU6 CAPL test로 unknown ID, wrong checksum, OTA request/response, seed/key, OTA checksum, overflow, lockout, ActiveSWVersion update를 검증하도록 구성되어 있습니다. |
| Trace32 CMM은 왜 필요한가? | CANoe는 bus 관점 증적이고, Trace32는 target 내부 변수와 breakpoint 관점 증적입니다. `blockedMsgCnt`, `otaState`, `securityLockoutActive` 같은 내부 상태를 실제 보드에서 보여줄 수 있습니다. |
| Complete와 Partial의 차이는 무엇인가? | 여기서 Complete는 코드/문서/test asset 수준으로 요구사항을 만족한다는 의미입니다. Partial은 구현 일부는 있으나 runtime evidence, full integration evidence, readable verdict 등이 부족하다는 의미입니다. |
| 실제 보드 실행 증적이 없으면 어떤 요구사항이 아직 Partial인가? | 대표적으로 `REQ-COM-001`, `REQ-COM-003`, `REQ-COM-004`, `REQ-INT-001`입니다. 특히 10분 안정 실행, CANoe readable verdict, Trace32 ISR screenshot, full six-board trace가 필요합니다. |
| OTA 보안은 어떻게 구현했는가? | `REQUEST_SEED 0x10`으로 seed를 받고, `SEND_KEY 0x11`에서 `seed XOR 0xA5` key가 맞으면 unlock됩니다. START는 unlock 전에는 거절되고, wrong key/AuthByte 3회 후 3000 ms lockout이 걸립니다. |
| OTA data buffer는 어떻게 보호되는가? | `OTA_DATA`는 Byte2 length, Byte3 AuthByte, Byte4-Byte7 payload 구조이고, 총 `256` bytes를 넘으면 overflow로 NACK `0x16`을 반환합니다. |
| checksum 실패 시 version이 바뀌는가? | 바뀌지 않습니다. `ActiveSWVersion`은 checksum 검증이 성공하고 `READY_TO_ACTIVATE` 상태에서 `ACTIVATE`가 성공할 때만 증가합니다. |
| `GatewayRxMask`는 무엇인가? | ECU1-ECU5의 정상 monitored frame을 한 번 이상 수신했는지 bit mask로 표시합니다. 모두 수신되면 `0x1F`가 됩니다. |
| `GatewayState`는 언제 NORMAL인가? | 모든 monitored ECU message를 받았고, alive/timeout/checksum fault가 없으면 `NORMAL`입니다. 아직 다 못 봤으면 `INIT`, fault가 있으면 `DEGRADED`입니다. |
| CANoe `.vtestreport`가 있으면 테스트 완료라고 말해도 되는가? | 조심해야 합니다. 파일은 있지만 현재 텍스트로 PASS/FAIL을 확인하지 못했으므로 “리포트 artifact는 존재하지만 readable verdict 또는 screenshot으로 실행 결과 확인이 추가 필요하다”고 말해야 합니다. |
| Trace32 CMM 파일이 있으면 실제 interrupt가 검증된 것인가? | 아닙니다. CMM은 검증 절차와 breakpoint/watch 설정을 제공하는 asset입니다. 실제 검증 완료로 말하려면 breakpoint hit screenshot이나 실행 로그가 필요합니다. |

## 9. 발표 전 추가로 모으면 좋은 증적

1. S32DS에서 build/flash 성공 screenshot.
2. UART boot log screenshot: `[time][ECU6][module][level] message`.
3. CANoe trace screenshot: `0x101`, `0x201`, `0x301`, `0x401`, `0x501`, `0x601` 동시 decode.
4. CANoe exported test report: `ECU6_Gateway_Test` PASS/FAIL, `ECU6_OTA_State_Test` PASS/FAIL.
5. Trace32 screenshot: `main`, `vPortTickISR`, CAN mailbox ISR, `COM_GatewayHandleRxMessage`, `COM_GatewayBuildStatusMessage`.
6. Gateway fault screenshot: unknown ID injection 후 `BlockedMsgCnt` 증가.
7. OTA screenshot: unlock, DATA, checksum OK, ACTIVATE 후 `ActiveSWVersion` 증가.
