import re
import unittest
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
COM_GATEWAY_C = ROOT / "Sources" / "services" / "com_gateway.c"

CAN_ID_ECU1_BODY = 0x101
CAN_ID_ECU2_POWER = 0x201
CAN_ID_ECU3_SENSOR = 0x301
CAN_ID_ECU4_CLUSTER = 0x401
CAN_ID_ECU5_DIAG = 0x501
CAN_ID_ECU6_GATEWAY = 0x601
CAN_ID_OTA_REQUEST = 0x650
CAN_ID_OTA_RESPONSE = 0x651

COM_RX_TIMEOUT_MS = 300
COM_ALIVE_FREEZE_LIMIT = 3
COM_ALIVE_COUNTER_BYTE = 6
COM_CHECKSUM_BYTE = 7
COM_ALIVE_COUNTER_MASK = 0x0F

OTA_DATA_BUFFER_SIZE = 256
OTA_DATA_AUTH_XOR = 0xC3
OTA_SECURITY_SEED_BASE = 0x5A
OTA_SECURITY_KEY_XOR = 0xA5
OTA_SECURITY_LOCKOUT_LIMIT = 3
OTA_SECURITY_LOCKOUT_MS = 3000

OTA_CMD_START = 0x01
OTA_CMD_DATA = 0x02
OTA_CMD_END = 0x03
OTA_CMD_ACTIVATE = 0x04
OTA_CMD_REQUEST_SEED = 0x10
OTA_CMD_SEND_KEY = 0x11

OTA_RESPONSE_ACK = 0x79
OTA_RESPONSE_NACK = 0x7F
OTA_DETAIL_ACCEPTED = 0x00
OTA_DETAIL_INVALID_SEQUENCE = 0x14
OTA_DETAIL_BUFFER_OVERFLOW = 0x16
OTA_DETAIL_CHECKSUM_MISMATCH = 0x17
OTA_DETAIL_INVALID_AUTH = 0x18
OTA_DETAIL_SECURITY_LOCKED = 0x20
OTA_DETAIL_INVALID_KEY = 0x22
OTA_DETAIL_SECURITY_LOCKOUT = 0x23

OTA_STATE_IDLE = 0
OTA_STATE_START = 1
OTA_STATE_RECEIVING = 2
OTA_STATE_VERIFYING = 3
OTA_STATE_READY_TO_ACTIVATE = 4
OTA_STATE_SUCCESS = 5
OTA_STATE_FAILED = 6


def source_define(name: str) -> int:
    source = COM_GATEWAY_C.read_text(encoding="utf-8", errors="ignore")
    match = re.search(rf"#define\s+{name}\s+\(?([xXa-fA-F0-9]+)U?L?\)?", source)
    if not match:
        raise AssertionError(f"{name} not found in {COM_GATEWAY_C}")
    return int(match.group(1), 0)


def checksum(can_id: int, data0_to_6: list[int]) -> int:
    value = can_id & 0xFF
    for item in data0_to_6:
        value ^= item & 0xFF
    return value & 0xFF


def make_status(can_id: int, alive: int, payload: list[int] | None = None) -> dict:
    data = [0] * 8
    if payload:
        for index, value in enumerate(payload[:6]):
            data[index] = value & 0xFF
    data[COM_ALIVE_COUNTER_BYTE] = alive & COM_ALIVE_COUNTER_MASK
    data[COM_CHECKSUM_BYTE] = checksum(can_id, data[:COM_CHECKSUM_BYTE])
    return {"id": can_id, "length": 8, "data": data}


@dataclass
class Monitor:
    can_id: int
    last_rx_ms: int = 0
    timeout_error: bool = False
    last_alive: int = 0
    alive_freeze_count: int = 0
    alive_error: bool = False
    alive_initialized: bool = False
    checksum_error: bool = False


class GatewayModel:
    def __init__(self) -> None:
        self.now_ms = 0
        self.blocked_msg_count = 0
        self.monitors = {
            can_id: Monitor(can_id)
            for can_id in [
                CAN_ID_ECU1_BODY,
                CAN_ID_ECU2_POWER,
                CAN_ID_ECU3_SENSOR,
                CAN_ID_ECU4_CLUSTER,
                CAN_ID_ECU5_DIAG,
            ]
        }

    def advance(self, ms: int) -> None:
        self.now_ms += ms

    def receive(self, msg: dict) -> None:
        monitor = self.monitors.get(msg["id"])
        if monitor is None:
            self.blocked_msg_count = min(0xFF, self.blocked_msg_count + 1)
            return

        if msg["length"] != 8 or msg["data"][7] != checksum(msg["id"], msg["data"][:7]):
            monitor.checksum_error = True
            self.blocked_msg_count = min(0xFF, self.blocked_msg_count + 1)
            return

        alive = msg["data"][COM_ALIVE_COUNTER_BYTE] & COM_ALIVE_COUNTER_MASK
        if not monitor.alive_initialized:
            monitor.last_alive = alive
            monitor.alive_initialized = True
            monitor.alive_freeze_count = 0
        elif alive == monitor.last_alive:
            monitor.alive_freeze_count = min(0xFF, monitor.alive_freeze_count + 1)
            if monitor.alive_freeze_count >= COM_ALIVE_FREEZE_LIMIT:
                monitor.alive_error = True
        else:
            monitor.last_alive = alive
            monitor.alive_freeze_count = 0
            monitor.alive_error = False

        monitor.last_rx_ms = self.now_ms
        monitor.timeout_error = False

    def check_timeouts(self) -> None:
        for monitor in self.monitors.values():
            if self.now_ms - monitor.last_rx_ms >= COM_RX_TIMEOUT_MS:
                monitor.timeout_error = True
                monitor.alive_initialized = False
                monitor.alive_freeze_count = 0


class OtaModel:
    def __init__(self) -> None:
        self.now_ms = 0
        self.ota_state = OTA_STATE_IDLE
        self.active_sw_version = 1
        self.security_unlocked = False
        self.security_locked = True
        self.security_seed = OTA_SECURITY_SEED_BASE
        self.security_seed_issued = False
        self.security_error_counter = 0
        self.security_failure_streak = 0
        self.lockout_active = False
        self.lockout_start_ms = 0
        self.response_counter = 0
        self.buffer = bytearray()
        self.verify_ok = False
        self.verify_pending = False

    def advance(self, ms: int) -> None:
        self.now_ms += ms
        self._update_lockout()

    def _update_lockout(self) -> None:
        if self.lockout_active and self.now_ms - self.lockout_start_ms >= OTA_SECURITY_LOCKOUT_MS:
            self.lockout_active = False
            self.security_failure_streak = 0
            self.security_unlocked = False
            self.security_locked = True
            self.security_seed_issued = False

    def _seed(self) -> int:
        return (
            OTA_SECURITY_SEED_BASE
            ^ self.response_counter
            ^ self.security_error_counter
            ^ self.active_sw_version
        ) & 0xFF

    def _record_failure(self) -> None:
        self.security_error_counter = min(0xFF, self.security_error_counter + 1)
        self.security_failure_streak = min(
            OTA_SECURITY_LOCKOUT_LIMIT, self.security_failure_streak + 1
        )
        if self.security_failure_streak >= OTA_SECURITY_LOCKOUT_LIMIT:
            self.lockout_active = True
            self.lockout_start_ms = self.now_ms
            self.security_unlocked = False
            self.security_locked = True
            self.security_seed_issued = False
            self.ota_state = OTA_STATE_FAILED

    def _build_response(self, command: int, sequence: int, code: int, detail: int) -> dict:
        state_for_response = self.ota_state
        response = {
            "id": CAN_ID_OTA_RESPONSE,
            "data": [
                code,
                command,
                sequence,
                detail,
                state_for_response & 0x07,
                self.active_sw_version,
                self.response_counter & 0x0F,
                0,
            ],
        }
        response["data"][7] = checksum(CAN_ID_OTA_RESPONSE, response["data"][:7])
        self.response_counter = (self.response_counter + 1) & 0x0F
        return response

    def publish_gateway_status(self) -> None:
        if self.verify_pending and self.ota_state == OTA_STATE_VERIFYING:
            self.verify_pending = False
            self.ota_state = OTA_STATE_READY_TO_ACTIVATE if self.verify_ok else OTA_STATE_FAILED

    def request(self, command: int, sequence: int, byte2: int = 0, byte3: int = 0, payload=None) -> dict:
        self._update_lockout()
        if payload is None:
            payload = []

        if self.lockout_active:
            return self._build_response(command, sequence, OTA_RESPONSE_NACK, OTA_DETAIL_SECURITY_LOCKOUT)

        if command == OTA_CMD_REQUEST_SEED:
            self.security_unlocked = False
            self.security_locked = True
            self.security_seed = self._seed()
            self.security_seed_issued = True
            self.buffer.clear()
            self.verify_ok = False
            self.verify_pending = False
            self.ota_state = OTA_STATE_IDLE
            return self._build_response(command, sequence, OTA_RESPONSE_ACK, self.security_seed)

        if command == OTA_CMD_SEND_KEY:
            if byte2 == (self.security_seed ^ OTA_SECURITY_KEY_XOR):
                self.security_unlocked = True
                self.security_locked = False
                self.security_seed_issued = False
                self.security_failure_streak = 0
                return self._build_response(command, sequence, OTA_RESPONSE_ACK, OTA_DETAIL_ACCEPTED)
            self._record_failure()
            self.security_unlocked = False
            self.security_locked = True
            self.security_seed_issued = False
            return self._build_response(command, sequence, OTA_RESPONSE_NACK, OTA_DETAIL_INVALID_KEY)

        if command == OTA_CMD_START:
            if not self.security_unlocked:
                return self._build_response(command, sequence, OTA_RESPONSE_NACK, OTA_DETAIL_SECURITY_LOCKED)
            self.buffer.clear()
            self.verify_ok = False
            self.verify_pending = False
            self.ota_state = OTA_STATE_START
            return self._build_response(command, sequence, OTA_RESPONSE_ACK, OTA_DETAIL_ACCEPTED)

        if command == OTA_CMD_DATA:
            if self.ota_state not in [OTA_STATE_START, OTA_STATE_RECEIVING]:
                self.ota_state = OTA_STATE_FAILED
                return self._build_response(command, sequence, OTA_RESPONSE_NACK, OTA_DETAIL_INVALID_SEQUENCE)

            payload_length = byte2
            expected_auth = self.security_seed ^ sequence ^ payload_length ^ OTA_DATA_AUTH_XOR
            if byte3 != expected_auth:
                self._record_failure()
                self.ota_state = OTA_STATE_FAILED
                return self._build_response(command, sequence, OTA_RESPONSE_NACK, OTA_DETAIL_INVALID_AUTH)

            if len(self.buffer) + payload_length > OTA_DATA_BUFFER_SIZE:
                self.ota_state = OTA_STATE_FAILED
                return self._build_response(command, sequence, OTA_RESPONSE_NACK, OTA_DETAIL_BUFFER_OVERFLOW)

            self.buffer.extend(payload[:payload_length])
            self.ota_state = OTA_STATE_RECEIVING
            return self._build_response(command, sequence, OTA_RESPONSE_ACK, OTA_DETAIL_ACCEPTED)

        if command == OTA_CMD_END:
            calculated = 0
            for item in self.buffer:
                calculated ^= item
            if calculated != byte2:
                self.verify_ok = False
                self.verify_pending = False
                self.ota_state = OTA_STATE_FAILED
                return self._build_response(command, sequence, OTA_RESPONSE_NACK, OTA_DETAIL_CHECKSUM_MISMATCH)
            self.verify_ok = True
            self.verify_pending = True
            self.ota_state = OTA_STATE_VERIFYING
            return self._build_response(command, sequence, OTA_RESPONSE_ACK, OTA_DETAIL_ACCEPTED)

        if command == OTA_CMD_ACTIVATE:
            if self.ota_state == OTA_STATE_READY_TO_ACTIVATE and self.verify_ok:
                self.active_sw_version = 1 if self.active_sw_version == 0xFF else self.active_sw_version + 1
                self.ota_state = OTA_STATE_SUCCESS
                return self._build_response(command, sequence, OTA_RESPONSE_ACK, OTA_DETAIL_ACCEPTED)
            self.ota_state = OTA_STATE_FAILED
            return self._build_response(command, sequence, OTA_RESPONSE_NACK, OTA_DETAIL_INVALID_SEQUENCE)

        return self._build_response(command, sequence, OTA_RESPONSE_NACK, 0x12)

    def unlock(self) -> int:
        seed_response = self.request(OTA_CMD_REQUEST_SEED, 1)
        seed = seed_response["data"][3]
        self.request(OTA_CMD_SEND_KEY, 2, seed ^ OTA_SECURITY_KEY_XOR)
        return seed


class TestEcu6GatewayLogic(unittest.TestCase):
    def test_source_constants_match_host_model(self) -> None:
        self.assertEqual(source_define("COM_RX_TIMEOUT_MS"), COM_RX_TIMEOUT_MS)
        self.assertEqual(source_define("COM_ALIVE_FREEZE_LIMIT"), COM_ALIVE_FREEZE_LIMIT)
        self.assertEqual(source_define("OTA_DATA_BUFFER_SIZE"), OTA_DATA_BUFFER_SIZE)
        self.assertEqual(source_define("OTA_SECURITY_LOCKOUT_MS"), OTA_SECURITY_LOCKOUT_MS)

    def test_checksum_calculation_and_validation(self) -> None:
        msg = make_status(CAN_ID_ECU3_SENSOR, alive=9, payload=[65, 70, 125, 128, 0, 0])
        self.assertEqual(msg["data"][7], checksum(CAN_ID_ECU3_SENSOR, msg["data"][:7]))

        gateway = GatewayModel()
        msg["data"][7] ^= 0xFF
        gateway.receive(msg)

        self.assertTrue(gateway.monitors[CAN_ID_ECU3_SENSOR].checksum_error)
        self.assertEqual(gateway.blocked_msg_count, 1)

    def test_unknown_id_increments_blocked_counter(self) -> None:
        gateway = GatewayModel()
        gateway.receive(make_status(0x123, alive=0))
        gateway.receive(make_status(0x321, alive=1))

        self.assertEqual(gateway.blocked_msg_count, 2)

    def test_alive_counter_freeze_and_recovery(self) -> None:
        gateway = GatewayModel()
        can_id = CAN_ID_ECU1_BODY
        for _ in range(COM_ALIVE_FREEZE_LIMIT + 1):
            gateway.receive(make_status(can_id, alive=4))
            gateway.advance(100)

        self.assertTrue(gateway.monitors[can_id].alive_error)

        gateway.receive(make_status(can_id, alive=5))
        self.assertFalse(gateway.monitors[can_id].alive_error)

    def test_timeout_detection_and_recovery(self) -> None:
        gateway = GatewayModel()
        can_id = CAN_ID_ECU2_POWER
        gateway.receive(make_status(can_id, alive=1))
        gateway.advance(COM_RX_TIMEOUT_MS)
        gateway.check_timeouts()

        self.assertTrue(gateway.monitors[can_id].timeout_error)

        gateway.receive(make_status(can_id, alive=2))
        self.assertFalse(gateway.monitors[can_id].timeout_error)

    def test_seed_key_unlock_and_start_gate(self) -> None:
        ota = OtaModel()
        denied = ota.request(OTA_CMD_START, 1)
        self.assertEqual(denied["data"][0], OTA_RESPONSE_NACK)
        self.assertEqual(denied["data"][3], OTA_DETAIL_SECURITY_LOCKED)

        ota.unlock()
        accepted = ota.request(OTA_CMD_START, 3)
        self.assertEqual(accepted["data"][0], OTA_RESPONSE_ACK)
        self.assertEqual(ota.ota_state, OTA_STATE_START)

    def test_ota_checksum_success_and_activation(self) -> None:
        ota = OtaModel()
        seed = ota.unlock()
        ota.request(OTA_CMD_START, 3)
        payload = [0x11, 0x22, 0x33, 0x44]
        auth = seed ^ 4 ^ len(payload) ^ OTA_DATA_AUTH_XOR
        data_response = ota.request(OTA_CMD_DATA, 4, len(payload), auth, payload)
        self.assertEqual(data_response["data"][0], OTA_RESPONSE_ACK)

        expected_checksum = 0
        for item in payload:
            expected_checksum ^= item
        end_response = ota.request(OTA_CMD_END, 5, expected_checksum)
        self.assertEqual(end_response["data"][0], OTA_RESPONSE_ACK)
        self.assertEqual(end_response["data"][4], OTA_STATE_VERIFYING)

        ota.publish_gateway_status()
        self.assertEqual(ota.ota_state, OTA_STATE_READY_TO_ACTIVATE)

        activate_response = ota.request(OTA_CMD_ACTIVATE, 6)
        self.assertEqual(activate_response["data"][0], OTA_RESPONSE_ACK)
        self.assertEqual(ota.active_sw_version, 2)
        self.assertEqual(ota.ota_state, OTA_STATE_SUCCESS)

    def test_ota_checksum_failure_keeps_version(self) -> None:
        ota = OtaModel()
        seed = ota.unlock()
        before_version = ota.active_sw_version
        ota.request(OTA_CMD_START, 3)
        payload = [0x30, 0x31]
        auth = seed ^ 4 ^ len(payload) ^ OTA_DATA_AUTH_XOR
        ota.request(OTA_CMD_DATA, 4, len(payload), auth, payload)

        failed = ota.request(OTA_CMD_END, 5, 0xFF)
        self.assertEqual(failed["data"][0], OTA_RESPONSE_NACK)
        self.assertEqual(failed["data"][3], OTA_DETAIL_CHECKSUM_MISMATCH)
        self.assertEqual(ota.active_sw_version, before_version)
        self.assertEqual(ota.ota_state, OTA_STATE_FAILED)

    def test_security_lockout_and_recovery(self) -> None:
        ota = OtaModel()
        for index in range(OTA_SECURITY_LOCKOUT_LIMIT):
            seed_response = ota.request(OTA_CMD_REQUEST_SEED, index * 2)
            seed = seed_response["data"][3]
            response = ota.request(OTA_CMD_SEND_KEY, index * 2 + 1, seed ^ 0xA4)
            self.assertEqual(response["data"][0], OTA_RESPONSE_NACK)

        self.assertTrue(ota.lockout_active)
        locked = ota.request(OTA_CMD_REQUEST_SEED, 99)
        self.assertEqual(locked["data"][3], OTA_DETAIL_SECURITY_LOCKOUT)

        ota.advance(OTA_SECURITY_LOCKOUT_MS)
        seed_again = ota.request(OTA_CMD_REQUEST_SEED, 100)
        self.assertEqual(seed_again["data"][0], OTA_RESPONSE_ACK)
        self.assertFalse(ota.lockout_active)


if __name__ == "__main__":
    unittest.main(verbosity=2)
