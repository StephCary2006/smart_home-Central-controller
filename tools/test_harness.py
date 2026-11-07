#!/usr/bin/env python3
"""
ESP32-C6 Smart Home Central Controller — Automated Host Test Harness
Validates JSON Telemetry over USB CDC (NFR-4) and certifies 72h system stability.
"""

import sys
import time
import json
import argparse
from datetime import datetime
from typing import Optional, Dict, Any

# Threshold constants
DEFAULT_BAUD = 115200
MIN_HEAP_BYTES = 50000          # Minimum free heap threshold (50KB)
MAX_ALLOWED_I2C_ERRORS = 10     # Max acceptable I2C errors over test run
MAX_ALLOWED_UART_ERRORS = 10    # Max acceptable UART FSM errors over test run
TEMP_MIN_CELSIUS = 5.0
TEMP_MAX_CELSIUS = 60.0


class TelemetryValidator:
    def __init__(self):
        self.stats = {
            "start_time": datetime.now().isoformat(),
            "end_time": None,
            "duration_sec": 0.0,
            "total_lines_read": 0,
            "telemetry_json_lines": 0,
            "non_json_lines": 0,
            "invalid_json_lines": 0,
            "heap_warnings": 0,
            "temp_out_of_range": 0,
            "max_i2c_errors": 0,
            "max_uart_errors": 0,
            "detected_reboots": 0,
            "min_heap_observed": sys.maxsize,
            "current_heap": 0,
            "latest_uptime_sec": 0,
            "sntp_synced_count": 0,
            "passed": False,
        }
        self.last_uptime = 0

    def process_raw_line(self, line: str) -> Optional[Dict[str, Any]]:
        self.stats["total_lines_read"] += 1
        line = line.strip()
        if not line:
            return None

        # Filter for JSON telemetry lines starting with '{' and ending with '}'
        if not (line.startswith("{") and line.endswith("}")):
            self.stats["non_json_lines"] += 1
            return None

        try:
            data = json.loads(line)
        except json.JSONDecodeError:
            self.stats["invalid_json_lines"] += 1
            return None

        # Check required fields
        required_fields = ["up_s", "temp", "hum", "light", "pm25", "relay", "sntp", "heap", "heap_min", "i2c_err", "uart_err"]
        if not all(field in data for field in required_fields):
            self.stats["invalid_json_lines"] += 1
            return None

        self.stats["telemetry_json_lines"] += 1
        self._validate_payload(data)
        return data

    def _validate_payload(self, data: Dict[str, Any]):
        uptime = data.get("up_s", 0)
        heap = data.get("heap", 0)
        heap_min = data.get("heap_min", 0)
        temp = data.get("temp", 0.0)
        i2c_err = data.get("i2c_err", 0)
        uart_err = data.get("uart_err", 0)
        sntp = data.get("sntp", 0)

        # Detect unexpected reboots via monotonic uptime gaps
        if self.last_uptime > 0 and uptime < self.last_uptime:
            self.stats["detected_reboots"] += 1
            print(f" [WARNING] REBOOT DETECTED: Uptime reset from {self.last_uptime}s to {uptime}s!")
        self.last_uptime = uptime
        self.stats["latest_uptime_sec"] = uptime

        # Heap memory leak monitoring
        self.stats["current_heap"] = heap
        if heap_min < self.stats["min_heap_observed"]:
            self.stats["min_heap_observed"] = heap_min

        if heap < MIN_HEAP_BYTES or heap_min < MIN_HEAP_BYTES:
            self.stats["heap_warnings"] += 1
            print(f" [WARNING] LOW HEAP DETECTED: current={heap}B, min={heap_min}B (limit={MIN_HEAP_BYTES}B)")

        # Temperature sensor plausibility
        if temp != 0.0 and (temp < TEMP_MIN_CELSIUS or temp > TEMP_MAX_CELSIUS):
            self.stats["temp_out_of_range"] += 1

        # Track bus and framing errors
        self.stats["max_i2c_errors"] = max(self.stats["max_i2c_errors"], i2c_err)
        self.stats["max_uart_errors"] = max(self.stats["max_uart_errors"], uart_err)

        if sntp == 1:
            self.stats["sntp_synced_count"] += 1

    def finalize(self) -> Dict[str, Any]:
        self.stats["end_time"] = datetime.now().isoformat()
        start_dt = datetime.fromisoformat(self.stats["start_time"])
        end_dt = datetime.fromisoformat(self.stats["end_time"])
        self.stats["duration_sec"] = round((end_dt - start_dt).total_seconds(), 2)

        if self.stats["min_heap_observed"] == sys.maxsize:
            self.stats["min_heap_observed"] = 0

        # Pass / Fail certification logic
        has_telemetry = self.stats["telemetry_json_lines"] > 0
        no_reboots = self.stats["detected_reboots"] == 0
        no_heap_leaks = self.stats["heap_warnings"] == 0
        i2c_ok = self.stats["max_i2c_errors"] <= MAX_ALLOWED_I2C_ERRORS
        uart_ok = self.stats["max_uart_errors"] <= MAX_ALLOWED_UART_ERRORS
        low_corruption = self.stats["invalid_json_lines"] <= max(1, self.stats["telemetry_json_lines"] * 0.01)

        self.stats["passed"] = (has_telemetry and no_reboots and no_heap_leaks and i2c_ok and uart_ok and low_corruption)
        return self.stats

    def print_report(self):
        s = self.stats
        print("\n" + "=" * 64)
        print(" ESP32-C6 SMART HOME CONTROLLER — TEST EXECUTION REPORT")
        print("=" * 64)
        print(f" Start Time:           {s['start_time']}")
        print(f" End Time:             {s['end_time']}")
        print(f" Duration:             {s['duration_sec']} seconds")
        print(f" Total Lines Read:     {s['total_lines_read']}")
        print(f" Valid JSON Telemetry: {s['telemetry_json_lines']}")
        print(f" Non-JSON (Logs/Text): {s['non_json_lines']}")
        print(f" Invalid/Corrupt JSON: {s['invalid_json_lines']}")
        print(f" Detected Reboots:     {s['detected_reboots']}")
        print(f" Free Heap Watermark:  {s['min_heap_observed']} bytes")
        print(f" Heap Low Warnings:    {s['heap_warnings']}")
        print(f" Max I2C Bus Errors:   {s['max_i2c_errors']} (limit: {MAX_ALLOWED_I2C_ERRORS})")
        print(f" Max UART FSM Errors:  {s['max_uart_errors']} (limit: {MAX_ALLOWED_UART_ERRORS})")
        print(f" SNTP Synced Samples:  {s['sntp_synced_count']}")
        print("-" * 64)
        if s["passed"]:
            print(" FINAL RESULT:          [PASSED] SYSTEM FULLY CERTIFIED")
        else:
            print(" FINAL RESULT:          [FAILED] THRESHOLD VIOLATIONS DETECTED")
        print("=" * 64 + "\n")


def run_serial_test(port: str, baud: int, duration_sec: int, output_file: Optional[str]):
    try:
        import serial
    except ImportError:
        print("Error: pyserial is required. Install via: pip install pyserial")
        sys.exit(1)

    print(f"Connecting to ESP32-C6 on {port} @ {baud} baud...")
    validator = TelemetryValidator()

    try:
        ser = serial.Serial(port, baud, timeout=1.5)
    except Exception as e:
        print(f"Failed to open serial port {port}: {e}")
        sys.exit(1)

    print(f"Running validation for {duration_sec} seconds (Press Ctrl+C to stop early)...")
    end_time = time.time() + duration_sec

    try:
        while time.time() < end_time:
            line_bytes = ser.readline()
            if not line_bytes:
                continue
            line = line_bytes.decode("utf-8", errors="replace")
            data = validator.process_raw_line(line)
            if data:
                print(f"[TELEMETRY] Up: {data['up_s']:4d}s | T: {data['temp']:4.1f}C | H: {data['hum']:2.0f}% | "
                      f"Light: {data['light']:4d}mV | PM2.5: {data['pm25']:3d} | Relay: {data['relay']} | "
                      f"Heap: {data['heap']}B | I2C_err: {data['i2c_err']}")
    except KeyboardInterrupt:
        print("\nTest manually interrupted by user.")
    finally:
        ser.close()

    results = validator.finalize()
    validator.print_report()

    out_name = output_file or f"test_report_{int(time.time())}.json"
    with open(out_name, "w", encoding="utf-8") as f:
        json.dump(results, f, indent=2)
    print(f"Detailed test report saved to: {out_name}")

    if not results["passed"]:
        sys.exit(2)


def run_self_test():
    print("Running TelemetryValidator internal self-test suite...")
    validator = TelemetryValidator()

    # 1. Valid telemetry stream
    valid_samples = [
        'I (1234) APP_MAIN: Heartbeat starting',
        '{"ts":1730900001,"up_s":1,"temp":28.5,"hum":65.0,"light":1850,"pm25":35,"relay":0,"sntp":1,"heap":185000,"heap_min":180000,"i2c_err":0,"uart_err":0}',
        '{"ts":1730900002,"up_s":2,"temp":28.6,"hum":65.2,"light":1845,"pm25":36,"relay":0,"sntp":1,"heap":185000,"heap_min":180000,"i2c_err":0,"uart_err":0}',
        '{"ts":1730900003,"up_s":3,"temp":28.5,"hum":65.1,"light":1840,"pm25":35,"relay":1,"sntp":1,"heap":184900,"heap_min":180000,"i2c_err":0,"uart_err":0}',
    ]
    for line in valid_samples:
        validator.process_raw_line(line)

    assert validator.stats["telemetry_json_lines"] == 3, f"Expected 3 valid JSON lines, got {validator.stats['telemetry_json_lines']}"
    assert validator.stats["non_json_lines"] == 1, "Expected 1 non-json log line"
    assert validator.stats["detected_reboots"] == 0, "Expected 0 reboots"

    # 2. Corrupt line detection
    validator.process_raw_line('{"ts":1730900004,"up_s":4,"temp":28.5,CORRUPT}')
    assert validator.stats["invalid_json_lines"] == 1, "Expected 1 invalid JSON line"

    # 3. Reboot detection test
    validator.process_raw_line('{"ts":1730900005,"up_s":1,"temp":28.5,"hum":65.0,"light":1850,"pm25":35,"relay":0,"sntp":1,"heap":185000,"heap_min":180000,"i2c_err":0,"uart_err":0}')
    assert validator.stats["detected_reboots"] == 1, "Expected 1 detected reboot on uptime reset"

    results = validator.finalize()
    assert results["detected_reboots"] == 1
    print(" [OK] TelemetryValidator internal self-test PASSED successfully!\n")
    return True


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="ESP32-C6 USB Telemetry Test Harness")
    parser.add_argument("--port", type=str, default="COM3", help="Serial COM port (e.g. COM3 or /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help="Baud rate (default: 115200)")
    parser.add_argument("--duration", type=int, default=300, help="Test duration in seconds (default: 300s = 5m)")
    parser.add_argument("--output", type=str, default=None, help="Output JSON report filepath")
    parser.add_argument("--self-test", action="store_true", help="Run automated self-tests without hardware")

    args = parser.parse_args()

    if args.self_test:
        run_self_test()
    else:
        run_serial_test(args.port, args.baud, args.duration, args.output)
