# ECU6 host-side tests

Run from the repository root:

```powershell
python -m unittest discover -s Tests/host -p "test_*.py" -v
```

These tests model the ECU6 gateway/OTA pure logic that can be validated without
MPC5748G hardware, CANoe, or Trace32:

- primary status XOR checksum validation
- unknown CAN ID blocked counter behavior
- alive counter freeze detection and recovery
- monitored ECU timeout detection and recovery
- OTA seed/key unlock and locked START rejection
- OTA payload checksum success/failure behavior
- security lockout and lockout timeout recovery

