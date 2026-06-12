#!/usr/bin/env python3
"""Capture MotemaSens WiFi CSV logs from the device IP."""

from __future__ import annotations

import argparse
import datetime as dt
import http.client
import pathlib
import time
import urllib.error
import urllib.request


def default_output_path() -> pathlib.Path:
    today = dt.datetime.now().strftime("%Y-%m-%d")
    stamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    return pathlib.Path("test_logs") / "wifi_captures" / today / f"wifi_capture_{stamp}.csv"


def get_text(url: str, timeout: float = 3.0) -> str:
    with urllib.request.urlopen(url, timeout=timeout) as response:
        return response.read().decode("utf-8", errors="replace")


def capture_stream(host: str, duration_s: float, output_path: pathlib.Path) -> int:
    stream_url = f"http://{host}/stream"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    line_count = 0
    end_time = time.monotonic() + duration_s

    with urllib.request.urlopen(stream_url, timeout=10) as response:
        with output_path.open("wb") as log_file:
            while time.monotonic() < end_time:
                line = response.readline()
                if not line:
                    break
                log_file.write(line)
                line_count += 1

    return line_count


def main() -> int:
    parser = argparse.ArgumentParser(description="Capture CSV from MotemaSens /stream endpoint.")
    parser.add_argument("--host", required=True, help="Device IP address, for example 192.168.5.29")
    parser.add_argument("--duration", type=float, default=10.0, help="Capture duration in seconds")
    parser.add_argument("--out", type=pathlib.Path, default=default_output_path(), help="Output CSV path")
    args = parser.parse_args()

    base_url = f"http://{args.host}"
    print(get_text(f"{base_url}/api/status").strip())
    rows = 0
    try:
        rows = capture_stream(args.host, args.duration, args.out)
    finally:
        try:
            print(get_text(f"{base_url}/api/stop").strip())
        except (ConnectionResetError, http.client.HTTPException, urllib.error.URLError) as exc:
            print(f"stop request failed: {exc}")

    print(f"saved {rows} lines to {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
