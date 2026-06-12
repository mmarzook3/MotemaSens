#!/usr/bin/env python3
"""Build an interactive HTML plot for the USB heart-sound live test."""

from __future__ import annotations

import argparse
import csv
import json
import re
from pathlib import Path


def read_clean_log(path: Path) -> list[dict[str, float]]:
    rows: list[dict[str, float]] = []
    with path.open(newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            rows.append({key: float(value) for key, value in row.items()})
    return rows


def read_beats(path: Path) -> list[dict[str, float]]:
    rows: list[dict[str, float]] = []
    with path.open(newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            rows.append({key: float(value) for key, value in row.items()})
    return rows


def read_raw_events(path: Path) -> tuple[list[str], list[dict[str, float]]]:
    metadata: list[str] = []
    accel_debug: list[dict[str, float]] = []
    pattern = re.compile(
        r"ACCEL_DEBUG,hz=(?P<hz>[-0-9.]+),fail=(?P<fail>[-0-9.]+),"
        r"raw=(?P<raw_x>-?\d+),(?P<raw_y>-?\d+),(?P<raw_z>-?\d+),"
        r"g=(?P<x>[-0-9.]+),(?P<y>[-0-9.]+),(?P<z>[-0-9.]+),mag=(?P<mag>[-0-9.]+)"
    )

    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("LIVE_TEST_") or line.startswith("LOG_HEADER"):
            metadata.append(line)
        match = pattern.search(line)
        if match:
            accel_debug.append({key: float(value) for key, value in match.groupdict().items()})
    return metadata, accel_debug


def column(rows: list[dict[str, float]], name: str) -> list[float]:
    return [row[name] for row in rows]


def stats(rows: list[dict[str, float]], beats: list[dict[str, float]]) -> dict[str, float]:
    duration_s = (rows[-1]["ms"] - rows[0]["ms"]) / 1000.0 if len(rows) > 1 else 0.0
    bpm_values = column(beats, "bpm") if beats else []
    return {
        "duration_s": round(duration_s, 2),
        "log_rows": len(rows),
        "beat_count": len(beats),
        "avg_bpm": round(sum(bpm_values) / len(bpm_values), 1) if bpm_values else 0.0,
        "last_bpm": round(bpm_values[-1], 1) if bpm_values else 0.0,
    }


def build_html(
    log_rows: list[dict[str, float]],
    beat_rows: list[dict[str, float]],
    metadata: list[str],
    accel_debug: list[dict[str, float]],
    output: Path,
) -> None:
    t = [ms / 1000.0 for ms in column(log_rows, "ms")]
    beat_t = [ms / 1000.0 for ms in column(beat_rows, "ms")]
    beat_levels = column(beat_rows, "level")
    summary = stats(log_rows, beat_rows)

    traces = [
        {
            "type": "scatter",
            "mode": "lines",
            "name": "Mic trace",
            "x": t,
            "y": column(log_rows, "mic_trace"),
            "xaxis": "x",
            "yaxis": "y",
            "line": {"color": "#d9f4ff", "width": 1.3},
        },
        {
            "type": "scatter",
            "mode": "lines",
            "name": "Mic level",
            "x": t,
            "y": column(log_rows, "mic_level"),
            "xaxis": "x2",
            "yaxis": "y2",
            "line": {"color": "#4cc9f0", "width": 1.4},
        },
        {
            "type": "scatter",
            "mode": "lines",
            "name": "Beat envelope",
            "x": t,
            "y": column(log_rows, "beat_envelope"),
            "xaxis": "x2",
            "yaxis": "y2",
            "line": {"color": "#f9c74f", "width": 1.4},
        },
        {
            "type": "scatter",
            "mode": "markers",
            "name": "Detected beats",
            "x": beat_t,
            "y": beat_levels,
            "xaxis": "x2",
            "yaxis": "y2",
            "marker": {"color": "#ff5a5f", "size": 8, "symbol": "diamond"},
            "customdata": [
                [int(row["interval_ms"]), round(row["bpm"], 1), round(row["gain"], 1)]
                for row in beat_rows
            ],
            "hovertemplate": "t=%{x:.2f}s<br>level=%{y:.3f}<br>interval=%{customdata[0]}ms<br>bpm=%{customdata[1]}<br>gain=%{customdata[2]}<extra>Beat</extra>",
        },
        {
            "type": "scatter",
            "mode": "lines",
            "name": "BPM estimate",
            "x": t,
            "y": column(log_rows, "bpm"),
            "xaxis": "x3",
            "yaxis": "y3",
            "line": {"color": "#80ed99", "width": 1.5},
        },
        {
            "type": "scatter",
            "mode": "lines",
            "name": "Accel X",
            "x": t,
            "y": column(log_rows, "acc_x_g"),
            "xaxis": "x4",
            "yaxis": "y4",
            "line": {"color": "#ff9f1c", "width": 1.2},
        },
        {
            "type": "scatter",
            "mode": "lines",
            "name": "Accel Y",
            "x": t,
            "y": column(log_rows, "acc_y_g"),
            "xaxis": "x4",
            "yaxis": "y4",
            "line": {"color": "#2ec4b6", "width": 1.2},
        },
        {
            "type": "scatter",
            "mode": "lines",
            "name": "Accel Z",
            "x": t,
            "y": column(log_rows, "acc_z_g"),
            "xaxis": "x4",
            "yaxis": "y4",
            "line": {"color": "#9b5de5", "width": 1.2},
        },
    ]

    debug_text = "<br>".join(metadata[:8])
    if accel_debug:
        avg_hz = sum(row["hz"] for row in accel_debug) / len(accel_debug)
        debug_text += f"<br>ACCEL_DEBUG rows: {len(accel_debug)}, avg Hz: {avg_hz:.1f}"

    layout = {
        "template": "plotly_dark",
        "title": {
            "text": (
                "MotemaSens 60s Heart Sound Live Test"
                f"<br><sup>{summary['beat_count']} beats, avg {summary['avg_bpm']} bpm, "
                f"last {summary['last_bpm']} bpm, {summary['log_rows']} samples</sup>"
            )
        },
        "height": 980,
        "margin": {"l": 70, "r": 30, "t": 95, "b": 55},
        "hovermode": "x unified",
        "legend": {"orientation": "h", "y": 1.04, "x": 0},
        "xaxis": {"domain": [0.0, 1.0], "anchor": "y", "matches": "x4", "showticklabels": False},
        "xaxis2": {"domain": [0.0, 1.0], "anchor": "y2", "matches": "x4", "showticklabels": False},
        "xaxis3": {"domain": [0.0, 1.0], "anchor": "y3", "matches": "x4", "showticklabels": False},
        "xaxis4": {"domain": [0.0, 1.0], "anchor": "y4", "title": "Time (s)"},
        "yaxis": {"domain": [0.76, 1.0], "title": "Mic trace"},
        "yaxis2": {"domain": [0.50, 0.72], "title": "Level / envelope"},
        "yaxis3": {"domain": [0.28, 0.45], "title": "BPM"},
        "yaxis4": {"domain": [0.0, 0.23], "title": "Accel (g)"},
        "annotations": [
            {
                "text": debug_text,
                "xref": "paper",
                "yref": "paper",
                "x": 1,
                "y": -0.11,
                "showarrow": False,
                "align": "right",
                "font": {"size": 11, "color": "#aab7c4"},
            }
        ],
    }

    html = f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>MotemaSens Heart Live Test</title>
  <script src="https://cdn.plot.ly/plotly-2.35.2.min.js"></script>
  <style>
    body {{ margin: 0; background: #111827; color: #e5edf5; font-family: Segoe UI, Arial, sans-serif; }}
    #plot {{ width: 100vw; height: 100vh; }}
  </style>
</head>
<body>
  <div id="plot"></div>
  <script>
    const traces = {json.dumps(traces)};
    const layout = {json.dumps(layout)};
    Plotly.newPlot('plot', traces, layout, {{responsive: true, scrollZoom: true}});
  </script>
</body>
</html>
"""
    output.write_text(html, encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raw", type=Path, default=Path("live_test_heart_60s.csv"))
    parser.add_argument("--log", type=Path, default=Path("test_logs/heart_live_60s_log.csv"))
    parser.add_argument("--beats", type=Path, default=Path("test_logs/heart_live_60s_beats.csv"))
    parser.add_argument("--output", type=Path, default=Path("test_logs/heart_live_60s_plot.html"))
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    log_rows = read_clean_log(args.log)
    beat_rows = read_beats(args.beats)
    metadata, accel_debug = read_raw_events(args.raw)
    build_html(log_rows, beat_rows, metadata, accel_debug, args.output)
    print(args.output.resolve())


if __name__ == "__main__":
    main()
