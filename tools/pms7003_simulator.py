#!/usr/bin/env python3
"""
PMS7003 UART Simulator & Fault-Injection Tool
Generates 32-byte frames according to PMS7003 serial protocol specification.
Used to validate the 5-state FSM parser on ESP32-C6 (GPIO 4 RX / GPIO 5 TX).
"""

import sys
import time
import struct
import random
import argparse
from typing import Tuple, List

START_BYTE_1 = 0x42
START_BYTE_2 = 0x4D
FRAME_LENGTH = 28 # 26 payload bytes + 2 checksum bytes


def build_pms7003_frame(pm1_0: int = 15, pm2_5: int = 35, pm10: int = 45) -> bytes:
    """
    Construct a standard 32-byte PMS7003 frame with valid 16-bit checksum.
    Payload words (big-endian):
    0: PM1.0 CF=1
    1: PM2.5 CF=1
    2: PM10  CF=1
    3: PM1.0 ATM
    4: PM2.5 ATM
    5: PM10  ATM
    6: 0.3um count
    7: 0.5um count
    8: 1.0um count
    9: 2.5um count
    10: 5.0um count
    11: 10um count
    12: Reserved / Version (0x0000)
    """
    header = bytes([START_BYTE_1, START_BYTE_2, 0x00, FRAME_LENGTH])
    payload = struct.pack(
        ">13H",
        pm1_0, pm2_5, pm10,
        pm1_0, pm2_5, pm10,
        pm1_0 * 10, pm2_5 * 10, pm10 * 10, 12, 4, 1, 0x0097
    )
    frame_no_chk = header + payload
    checksum = sum(frame_no_chk) & 0xFFFF
    return frame_no_chk + struct.pack(">H", checksum)


def inject_noise_into_frame(valid_frame: bytes) -> bytes:
    """Deliberately introduce framing flaws to test 5-State FSM recovery."""
    flaw_type = random.choice(["stray_prefix", "corrupt_checksum", "truncated", "random_junk"])

    if flaw_type == "stray_prefix":
        # Insert stray 0x42 bytes before true frame
        return bytes([0x42, 0xAA, 0x42]) + valid_frame
    elif flaw_type == "corrupt_checksum":
        # Corrupt the last byte
        corrupt = bytearray(valid_frame)
        corrupt[-1] ^= 0xFF
        return bytes(corrupt)
    elif flaw_type == "truncated":
        # Send only first 15 bytes, then next loop sends valid
        return valid_frame[:15]
    else:
        # Emit random garbage bytes
        return bytes([random.randint(0, 255) for _ in range(8)]) + valid_frame


def run_simulator(port: str, baud: int = 9600, interval_sec: float = 1.0, inject_faults: bool = False):
    try:
        import serial
    except ImportError:
        print("Error: pyserial required. Install with: pip install pyserial")
        sys.exit(1)

    print(f"Opening serial port {port} at {baud} baud (PMS7003 simulator)...")
    try:
        ser = serial.Serial(port, baud, timeout=1)
    except Exception as e:
        print(f"Error opening port {port}: {e}")
        sys.exit(1)

    print(f"Transmitting PMS7003 frames every {interval_sec}s (Fault injection: {'ENABLED' if inject_faults else 'DISABLED'})...")
    frame_count = 0
    pm25_val = 20

    try:
        while True:
            # Vary PM2.5 gradually
            pm25_val = max(10, min(180, pm25_val + random.randint(-2, 3)))
            frame = build_pms7003_frame(pm1_0=int(pm25_val * 0.6), pm2_5=pm25_val, pm10=int(pm25_val * 1.3))

            if inject_faults and random.random() < 0.25:
                tx_data = inject_noise_into_frame(frame)
                print(f"[FAUL-INJECT] Sending noisy/corrupted frame ({len(tx_data)} bytes)")
            else:
                tx_data = frame
                print(f"[FRAME #{frame_count:04d}] Transmitted: PM1.0={int(pm25_val*0.6)} ug/m3, PM2.5={pm25_val} ug/m3, PM10={int(pm25_val*1.3)} ug/m3")

            ser.write(tx_data)
            frame_count += 1
            time.sleep(interval_sec)
    except KeyboardInterrupt:
        print("\nSimulator stopped by user.")
    finally:
        ser.close()


def run_self_test():
    print("Running PMS7003 Simulator self-test...")
    frame = build_pms7003_frame(pm1_0=10, pm2_5=25, pm10=40)
    assert len(frame) == 32, f"Expected 32-byte frame, got {len(frame)}"
    assert frame[0] == 0x42 and frame[1] == 0x4D, "Header must be 0x42 0x4D"
    length = (frame[2] << 8) | frame[3]
    assert length == 28, f"Length must be 28, got {length}"

    calculated_sum = sum(frame[:30]) & 0xFFFF
    expected_sum = (frame[30] << 8) | frame[31]
    assert calculated_sum == expected_sum, f"Checksum mismatch: calc=0x{calculated_sum:04X}, exp=0x{expected_sum:04X}"

    # Verify noisy frame generation
    noisy = inject_noise_into_frame(frame)
    assert len(noisy) > 0, "Noisy frame must contain data"
    print(" [OK] PMS7003 Simulator self-test PASSED successfully!\n")
    return True


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="PMS7003 UART Serial Simulator & Fault Injector")
    parser.add_argument("--port", type=str, default="COM4", help="USB-UART COM port (e.g. COM4)")
    parser.add_argument("--baud", type=int, default=9600, help="PMS7003 baud rate (default: 9600)")
    parser.add_argument("--interval", type=float, default=1.0, help="Transmission interval in seconds")
    parser.add_argument("--inject-noise", action="store_true", help="Inject intermittent noise & corrupted frames")
    parser.add_argument("--self-test", action="store_true", help="Run automated offline self-test")

    args = parser.parse_args()

    if args.self_test:
        run_self_test()
    else:
        run_simulator(args.port, args.baud, args.interval, args.inject_noise)
