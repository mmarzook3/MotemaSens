#!/usr/bin/env python3
"""Run USB mic calibration captures with optional PC generated tones."""

from __future__ import annotations

import argparse
import csv
import math
import re
import statistics
import threading
import time
import wave
from pathlib import Path

import serial

try:
    import winsound
except ImportError:  # pragma: no cover - this tool is for Windows lab use.
    winsound = None


LOG_RE = re.compile(r"^LOG,")
BEAT_RE = re.compile(
    r"^BEAT,(\d+),interval_ms=(\d+),bpm=([0-9.]+),level=([0-9.]+),gain=([0-9.]+)(?:,delay_ms=(\d+))?"
)
END_RE = re.compile(
    r"^LIVE_TEST_END,reason=([^,]+),elapsed_ms=(\d+),samples=(\d+),beats=(\d+),rejected=(\d+),bpm=([0-9.]+)"
)


def play_tone(frequency_hz: int, duration_s: int, volume: float, output_dir: Path) -> None:
    if winsound is None:
        raise RuntimeError("winsound is required to generate tones on Windows")
    sample_rate = 44100
    sample_count = int(sample_rate * duration_s)
    amplitude = int(max(0.0, min(volume, 1.0)) * 32767)
    wav_path = output_dir / f"generated_{frequency_hz}hz_{duration_s}s.wav"
    with wave.open(str(wav_path), "wb") as handle:
        handle.setnchannels(1)
        handle.setsampwidth(2)
        handle.setframerate(sample_rate)
        frames = bytearray()
        for index in range(sample_count):
            sample = int(amplitude * math.sin(2.0 * math.pi * frequency_hz * index / sample_rate))
            frames.extend(sample.to_bytes(2, byteorder="little", signed=True))
        handle.writeframes(frames)
    winsound.PlaySound(str(wav_path), winsound.SND_FILENAME)


def capture(port: str, output: Path, duration_s: int, frequency_hz: int | None, tone_volume: float) -> list[str]:
    tone_thread = None
    ser = serial.Serial(port, 115200, timeout=0.2)
    try:
      time.sleep(1.0)
      ser.reset_input_buffer()
      if frequency_hz is not None:
          tone_thread = threading.Thread(target=play_tone, args=(frequency_hz, duration_s, tone_volume, output.parent), daemon=True)
          tone_thread.start()
          time.sleep(0.15)
      ser.write(b"S")
      ser.flush()

      lines: list[str] = []
      started = time.time()
      while True:
          line = ser.readline()
          now = time.time()
          if line:
              text = line.decode("utf-8", "replace").rstrip()
              lines.append(text)
              if text.startswith("LIVE_TEST_END"):
                  break
          if now - started > duration_s + 20:
              ser.write(b"X")
              ser.flush()
              lines.append("CAPTURE_TIMEOUT")
              break
    finally:
      ser.close()
      if tone_thread:
          tone_thread.join(timeout=1.0)

    output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return lines


def summarize(raw_path: Path) -> dict[str, float | int | str]:
    logs: list[list[float]] = []
    beats: list[list[float]] = []
    end_line = ""
    for line in raw_path.read_text(encoding="utf-8", errors="replace").splitlines():
        if LOG_RE.match(line):
            parts = line.split(",")
            if len(parts) == 14:
                try:
                    logs.append([float(value) for value in parts[1:11]])
                except ValueError:
                    pass
        else:
            beat = BEAT_RE.match(line)
            if beat:
                beats.append([float(value) for value in beat.groups(default="0")])
            elif line.startswith("LIVE_TEST_END"):
                end_line = line

    result: dict[str, float | int | str] = {
        "file": raw_path.name,
        "rows": len(logs),
        "beats": len(beats),
        "rejected": 0,
        "final_bpm": 0.0,
        "end": end_line,
    }
    end_match = END_RE.match(end_line)
    if end_match:
        result["beats"] = int(end_match.group(4))
        result["rejected"] = int(end_match.group(5))
        result["final_bpm"] = float(end_match.group(6))
    if logs:
        ms = [row[0] for row in logs]
        duration = (ms[-1] - ms[0]) / 1000.0 if len(ms) > 1 else 0.0
        result["rate_hz"] = (len(ms) - 1) / duration if duration > 0 else 0.0
        for index, name in [(1, "mic_trace"), (2, "mic_level"), (3, "envelope"), (4, "threshold"), (5, "motion")]:
            values = [row[index] for row in logs]
            result[f"{name}_mean"] = statistics.mean(values)
            result[f"{name}_max"] = max(values)
            result[f"{name}_sd"] = statistics.pstdev(values)
        result["mic_level_saturation_pct"] = 100.0 * sum(1 for row in logs if row[2] >= 0.995) / len(logs)
    if beats:
        intervals = [row[1] for row in beats]
        result["interval_mean_ms"] = statistics.mean(intervals)
        result["interval_min_ms"] = min(intervals)
        result["interval_max_ms"] = max(intervals)
    return result


def write_reports(output_dir: Path, rows: list[dict[str, float | int | str]]) -> None:
    csv_path = output_dir / "frequency_calibration_summary.csv"
    md_path = output_dir / "frequency_calibration_report.md"
    fieldnames = [
        "test",
        "frequency_hz",
        "file",
        "rows",
        "rate_hz",
        "beats",
        "rejected",
        "final_bpm",
        "mic_trace_sd",
        "mic_level_mean",
        "mic_level_max",
        "mic_level_saturation_pct",
        "envelope_mean",
        "envelope_max",
        "threshold_mean",
        "motion_max",
        "interval_mean_ms",
        "interval_min_ms",
        "interval_max_ms",
        "end",
    ]
    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)

    lines = [
        "# Frequency Calibration Report",
        "",
        "Each capture is 60 seconds over USB at the firmware 100 Hz log rate.",
        "",
        "| Test | Hz | Rows | Rate Hz | Beats | Rejected | Mic level mean | Saturated % | Envelope mean | Threshold mean | Motion max |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for row in rows:
        lines.append(
            "| {test} | {frequency_hz} | {rows} | {rate_hz:.2f} | {beats} | {rejected} | {mic_level_mean:.4f} | {mic_level_saturation_pct:.1f} | {envelope_mean:.4f} | {threshold_mean:.4f} | {motion_max:.4f} |".format(
                **{key: row.get(key, 0) for key in fieldnames}
            )
        )
    lines.append("")
    lines.append("Use steady tones to confirm the detector does not report false heartbeats for continuous sound.")
    lines.append("Use the ambient row as the room and electronics noise floor for later threshold tuning.")
    lines.append("Rows with high `mic_level_saturation_pct` are too loud for frequency response calibration and should be repeated at lower volume or farther from the speaker.")
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM14")
    parser.add_argument("--duration", type=int, default=60)
    parser.add_argument("--output-dir", default=r"C:\codex\MotemaSens\test_logs\frequency_calibration")
    parser.add_argument("--frequencies", default="60,80,100,120,150,200,250,300")
    parser.add_argument("--tone-volume", type=float, default=0.18, help="Generated sine amplitude from 0.0 to 1.0")
    args = parser.parse_args()

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    plan: list[tuple[str, int | None]] = [("ambient", None)]
    plan.extend((f"tone_{freq}hz", freq) for freq in [int(item) for item in args.frequencies.split(",") if item.strip()])

    summaries: list[dict[str, float | int | str]] = []
    for name, frequency in plan:
        raw_path = output_dir / f"{name}_100hz_60s.csv"
        print(f"CAPTURE {name} -> {raw_path}")
        capture(args.port, raw_path, args.duration, frequency, args.tone_volume)
        summary = summarize(raw_path)
        summary["test"] = name
        summary["frequency_hz"] = frequency or 0
        summaries.append(summary)
        print(f"  rows={summary.get('rows')} beats={summary.get('beats')} end={summary.get('end')}")

    write_reports(output_dir, summaries)
    print(f"REPORT {output_dir / 'frequency_calibration_report.md'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
