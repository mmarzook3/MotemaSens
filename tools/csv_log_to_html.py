#!/usr/bin/env python3
"""Convert a MotemaSens logger CSV into a self-contained HTML plot."""

from __future__ import annotations

import argparse
import csv
import html
import json
import pathlib
from typing import Any


DEFAULT_CHANNELS = [
    "mic_trace",
    "mic_level",
    "beat_envelope",
    "beat_threshold",
    "motion_level",
    "bpm",
    "acc_x_g",
    "acc_y_g",
    "acc_z_g",
    "ecg_ch1",
    "ecg_ch2",
    "ecg_ch3",
    "ecg_ch4",
]


def parse_value(value: str) -> Any:
    value = value.strip()
    if value == "":
        return None
    try:
        if value.startswith(("0x", "0X")):
            return int(value, 16)
        return int(value)
    except ValueError:
        pass
    try:
        return float(value)
    except ValueError:
        return value


def read_log_csv(path: pathlib.Path) -> tuple[list[str], list[dict[str, Any]]]:
    header: list[str] | None = None
    rows: list[dict[str, Any]] = []

    with path.open(newline="", encoding="utf-8-sig") as csv_file:
        reader = csv.reader(csv_file)
        for raw_row in reader:
            if not raw_row:
                continue
            row_type = raw_row[0].strip()
            if row_type == "LOG_HEADER":
                header = raw_row
                continue
            if row_type != "LOG":
                continue
            if header is None:
                raise ValueError("CSV does not contain LOG_HEADER before LOG rows")
            if len(raw_row) < len(header):
                raw_row.extend([""] * (len(header) - len(raw_row)))
            parsed = {key: parse_value(raw_row[index]) for index, key in enumerate(header)}
            rows.append(parsed)

    if header is None:
        raise ValueError("CSV does not contain LOG_HEADER")
    if not rows:
        raise ValueError("CSV does not contain any LOG rows")
    return header, rows


def numeric_channels(header: list[str], rows: list[dict[str, Any]]) -> list[str]:
    channels: list[str] = []
    for key in header:
        if key in {"LOG_HEADER", "LOG", "ecg_status"}:
            continue
        values = [row.get(key) for row in rows[:20]]
        if any(isinstance(value, (int, float)) for value in values):
            channels.append(key)
    return channels


def build_summary(input_path: pathlib.Path, rows: list[dict[str, Any]]) -> dict[str, Any]:
    ms_values = [row["ms"] for row in rows if isinstance(row.get("ms"), (int, float))]
    ecg_seq_values = [row["ecg_seq"] for row in rows if isinstance(row.get("ecg_seq"), (int, float))]
    sample_deltas = [b - a for a, b in zip(ms_values, ms_values[1:])]
    duration_ms = (ms_values[-1] - ms_values[0]) if len(ms_values) > 1 else 0
    rate_hz = (len(ms_values) * 1000.0 / duration_ms) if duration_ms else 0.0

    summary: dict[str, Any] = {
        "source": input_path.name,
        "rows": len(rows),
        "first_ms": ms_values[0] if ms_values else None,
        "last_ms": ms_values[-1] if ms_values else None,
        "duration_ms": duration_ms,
        "rate_hz": round(rate_hz, 2),
    }
    if sample_deltas:
        summary.update(
            {
                "dt_min": min(sample_deltas),
                "dt_max": max(sample_deltas),
                "dt_avg": round(sum(sample_deltas) / len(sample_deltas), 2),
            }
        )
    if len(ecg_seq_values) > 1:
        summary["ecg_seq_delta"] = ecg_seq_values[-1] - ecg_seq_values[0]
    return summary


def write_html(output_path: pathlib.Path, source_path: pathlib.Path, rows: list[dict[str, Any]], channels: list[str]) -> None:
    selected_channels = [channel for channel in DEFAULT_CHANNELS if channel in channels]
    if not selected_channels:
        selected_channels = channels[:8]
    summary = build_summary(source_path, rows)
    payload = {
        "summary": summary,
        "channels": channels,
        "selectedChannels": selected_channels,
        "rows": rows,
    }

    title = f"MotemaSens CSV Plot - {source_path.name}"
    html_text = HTML_TEMPLATE.replace("__TITLE__", html.escape(title)).replace(
        "__PAYLOAD__", json.dumps(payload, separators=(",", ":"))
    )
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(html_text, encoding="utf-8")


HTML_TEMPLATE = r"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>__TITLE__</title>
<style>
:root { color-scheme: dark; --bg: #0d1117; --panel: #161b22; --text: #e6edf3; --muted: #8b949e; --grid: #30363d; }
* { box-sizing: border-box; }
body { margin: 0; font-family: "Segoe UI", Arial, sans-serif; background: var(--bg); color: var(--text); }
header { padding: 16px 18px 10px; border-bottom: 1px solid #30363d; background: #0b1016; position: sticky; top: 0; z-index: 2; }
h1 { margin: 0 0 8px; font-size: 20px; font-weight: 650; }
.summary { display: flex; flex-wrap: wrap; gap: 8px; color: var(--muted); font-size: 13px; }
.pill { padding: 4px 8px; border: 1px solid #30363d; border-radius: 999px; background: #111820; }
main { display: grid; grid-template-columns: 260px minmax(0, 1fr); min-height: calc(100vh - 82px); }
aside { border-right: 1px solid #30363d; padding: 14px; background: var(--panel); overflow: auto; }
.plot-wrap { padding: 14px; min-width: 0; }
canvas { width: 100%; height: calc(100vh - 138px); display: block; background: #05080c; border: 1px solid #30363d; }
label { display: flex; align-items: center; gap: 8px; margin: 8px 0; color: #d6dee8; font-size: 13px; }
input[type="checkbox"] { width: 16px; height: 16px; }
.buttons { display: flex; gap: 8px; margin-bottom: 12px; }
button { background: #238636; color: white; border: 0; border-radius: 6px; padding: 7px 10px; cursor: pointer; }
button.secondary { background: #30363d; }
.hint, #readout { color: var(--muted); font-size: 12px; line-height: 1.45; margin-top: 12px; white-space: pre-wrap; }
.legend { display: grid; gap: 4px; margin-top: 10px; font-size: 12px; }
.legend div { display: flex; align-items: center; gap: 7px; }
.swatch { width: 18px; height: 3px; border-radius: 99px; display: inline-block; }
@media (max-width: 760px) {
  main { grid-template-columns: 1fr; }
  aside { border-right: 0; border-bottom: 1px solid #30363d; max-height: 260px; }
  canvas { height: 58vh; }
}
</style>
</head>
<body>
<header>
  <h1>MotemaSens CSV Plot</h1>
  <div id="summary" class="summary"></div>
</header>
<main>
  <aside>
    <div class="buttons">
      <button id="selectMain">Main</button>
      <button id="selectAll" class="secondary">All</button>
      <button id="clearAll" class="secondary">Clear</button>
    </div>
    <div id="channelList"></div>
    <div id="readout">Move mouse over the graph to inspect values.</div>
    <div class="hint">Mouse wheel zooms time. Drag pans. Double click resets zoom.</div>
  </aside>
  <section class="plot-wrap">
    <canvas id="plot"></canvas>
    <div id="legend" class="legend"></div>
  </section>
</main>
<script>
const DATA = __PAYLOAD__;
const rows = DATA.rows;
const channels = DATA.channels;
const colors = ["#58a6ff","#f78166","#7ee787","#d2a8ff","#f2cc60","#a5d6ff","#ff7b72","#56d364","#e3b341","#ffa657","#bc8cff","#79c0ff"];
let selected = new Set(DATA.selectedChannels);
let viewStart = rows[0].ms;
let viewEnd = rows[rows.length - 1].ms;
const fullStart = viewStart;
const fullEnd = viewEnd;
let drag = null;
const canvas = document.getElementById("plot");
const ctx = canvas.getContext("2d");

function fmt(value) {
  if (typeof value !== "number") return value;
  if (Math.abs(value) >= 1000) return value.toFixed(0);
  if (Math.abs(value) >= 10) return value.toFixed(2);
  return value.toFixed(4);
}

function updateSummary() {
  const summary = DATA.summary;
  document.getElementById("summary").innerHTML = Object.entries(summary)
    .map(([key, value]) => `<span class="pill">${key}: ${value}</span>`).join("");
}

function buildControls() {
  const list = document.getElementById("channelList");
  list.innerHTML = channels.map((channel, index) => {
    const checked = selected.has(channel) ? "checked" : "";
    const color = colors[index % colors.length];
    return `<label><input type="checkbox" data-channel="${channel}" ${checked}><span class="swatch" style="background:${color}"></span>${channel}</label>`;
  }).join("");
  list.querySelectorAll("input").forEach(input => {
    input.addEventListener("change", () => {
      if (input.checked) selected.add(input.dataset.channel);
      else selected.delete(input.dataset.channel);
      draw();
    });
  });
  document.getElementById("selectMain").onclick = () => setSelected(DATA.selectedChannels);
  document.getElementById("selectAll").onclick = () => setSelected(channels);
  document.getElementById("clearAll").onclick = () => setSelected([]);
}

function setSelected(next) {
  selected = new Set(next);
  document.querySelectorAll("#channelList input").forEach(input => input.checked = selected.has(input.dataset.channel));
  draw();
}

function visibleRows() {
  return rows.filter(row => row.ms >= viewStart && row.ms <= viewEnd);
}

function channelRange(data, channel) {
  let min = Infinity;
  let max = -Infinity;
  data.forEach(row => {
    const value = row[channel];
    if (typeof value === "number" && Number.isFinite(value)) {
      if (value < min) min = value;
      if (value > max) max = value;
    }
  });
  if (!Number.isFinite(min) || !Number.isFinite(max)) return [-1, 1];
  if (min === max) return [min - 1, max + 1];
  const pad = (max - min) * 0.08;
  return [min - pad, max + pad];
}

function resizeCanvas() {
  const rect = canvas.getBoundingClientRect();
  const scale = window.devicePixelRatio || 1;
  canvas.width = Math.max(320, Math.floor(rect.width * scale));
  canvas.height = Math.max(220, Math.floor(rect.height * scale));
  ctx.setTransform(scale, 0, 0, scale, 0, 0);
}

function draw() {
  resizeCanvas();
  const rect = canvas.getBoundingClientRect();
  const w = rect.width;
  const h = rect.height;
  const left = 62, right = 18, top = 18, bottom = 32;
  const plotW = w - left - right;
  const plotH = h - top - bottom;
  ctx.clearRect(0, 0, w, h);
  ctx.fillStyle = "#05080c";
  ctx.fillRect(0, 0, w, h);
  ctx.strokeStyle = "#30363d";
  ctx.lineWidth = 1;
  ctx.font = "12px Segoe UI, Arial";
  ctx.fillStyle = "#8b949e";

  for (let i = 0; i <= 10; i++) {
    const x = left + plotW * i / 10;
    ctx.beginPath(); ctx.moveTo(x, top); ctx.lineTo(x, top + plotH); ctx.stroke();
    const ms = viewStart + (viewEnd - viewStart) * i / 10;
    ctx.fillText(`${(ms / 1000).toFixed(2)}s`, x - 14, top + plotH + 22);
  }
  for (let i = 0; i <= 6; i++) {
    const y = top + plotH * i / 6;
    ctx.beginPath(); ctx.moveTo(left, y); ctx.lineTo(left + plotW, y); ctx.stroke();
  }

  const data = visibleRows();
  const active = channels.filter(channel => selected.has(channel));
  const ranges = Object.fromEntries(active.map(channel => [channel, channelRange(data, channel)]));
  active.forEach((channel, index) => {
    const [min, max] = ranges[channel];
    const color = colors[channels.indexOf(channel) % colors.length];
    ctx.strokeStyle = color;
    ctx.lineWidth = 1.35;
    ctx.beginPath();
    let started = false;
    data.forEach(row => {
      const value = row[channel];
      if (typeof value !== "number" || !Number.isFinite(value)) return;
      const x = left + ((row.ms - viewStart) / (viewEnd - viewStart || 1)) * plotW;
      const y = top + (1 - (value - min) / (max - min || 1)) * plotH;
      if (!started) { ctx.moveTo(x, y); started = true; }
      else ctx.lineTo(x, y);
    });
    ctx.stroke();
  });

  document.getElementById("legend").innerHTML = active.map(channel => {
    const range = ranges[channel] || [0, 0];
    const color = colors[channels.indexOf(channel) % colors.length];
    return `<div><span class="swatch" style="background:${color}"></span>${channel}: ${fmt(range[0])} to ${fmt(range[1])}</div>`;
  }).join("");
}

function rowAtX(clientX) {
  const rect = canvas.getBoundingClientRect();
  const left = 62, right = 18;
  const x = Math.max(left, Math.min(rect.width - right, clientX - rect.left));
  const ms = viewStart + ((x - left) / (rect.width - left - right)) * (viewEnd - viewStart);
  let best = rows[0], bestDistance = Infinity;
  for (const row of rows) {
    const distance = Math.abs(row.ms - ms);
    if (distance < bestDistance) { best = row; bestDistance = distance; }
  }
  return best;
}

canvas.addEventListener("mousemove", event => {
  if (drag) {
    const dx = event.clientX - drag.x;
    const span = drag.end - drag.start;
    const rect = canvas.getBoundingClientRect();
    const shift = -dx / rect.width * span;
    viewStart = Math.max(fullStart, drag.start + shift);
    viewEnd = Math.min(fullEnd, drag.end + shift);
    if (viewEnd - viewStart < span) {
      if (viewStart === fullStart) viewEnd = fullStart + span;
      if (viewEnd === fullEnd) viewStart = fullEnd - span;
    }
    draw();
    return;
  }
  const row = rowAtX(event.clientX);
  const active = channels.filter(channel => selected.has(channel));
  document.getElementById("readout").textContent = [`ms: ${row.ms}`].concat(active.map(channel => `${channel}: ${fmt(row[channel])}`)).join("\n");
});
canvas.addEventListener("mousedown", event => { drag = { x: event.clientX, start: viewStart, end: viewEnd }; });
window.addEventListener("mouseup", () => { drag = null; });
canvas.addEventListener("dblclick", () => { viewStart = fullStart; viewEnd = fullEnd; draw(); });
canvas.addEventListener("wheel", event => {
  event.preventDefault();
  const row = rowAtX(event.clientX);
  const center = row.ms;
  const span = viewEnd - viewStart;
  const nextSpan = Math.max(50, Math.min(fullEnd - fullStart, span * (event.deltaY > 0 ? 1.18 : 0.82)));
  viewStart = Math.max(fullStart, center - nextSpan / 2);
  viewEnd = Math.min(fullEnd, center + nextSpan / 2);
  if (viewEnd - viewStart < nextSpan) {
    if (viewStart === fullStart) viewEnd = fullStart + nextSpan;
    if (viewEnd === fullEnd) viewStart = fullEnd - nextSpan;
  }
  draw();
}, { passive: false });

window.addEventListener("resize", draw);
updateSummary();
buildControls();
draw();
</script>
</body>
</html>
"""


def main() -> int:
    parser = argparse.ArgumentParser(description="Convert MotemaSens CSV logs to an interactive HTML plot.")
    parser.add_argument("csv", type=pathlib.Path, help="Input CSV downloaded from /stream")
    parser.add_argument("-o", "--out", type=pathlib.Path, help="Output HTML path")
    args = parser.parse_args()

    input_path = args.csv
    output_path = args.out or input_path.with_suffix(".html")
    header, rows = read_log_csv(input_path)
    channels = numeric_channels(header, rows)
    write_html(output_path, input_path, rows, channels)
    print(f"wrote {output_path} with {len(rows)} rows and {len(channels)} channels")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
