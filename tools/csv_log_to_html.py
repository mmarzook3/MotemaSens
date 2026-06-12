#!/usr/bin/env python3
"""Convert a MotemaSens logger CSV into a clean self-contained HTML plot."""

from __future__ import annotations

import argparse
import csv
import html
import json
import pathlib
from typing import Any


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
            rows.append({key: parse_value(raw_row[index]) for index, key in enumerate(header)})

    if header is None:
        raise ValueError("CSV does not contain LOG_HEADER")
    if not rows:
        raise ValueError("CSV does not contain any LOG rows")
    add_derived_channels(rows)
    return header, rows


def add_derived_channels(rows: list[dict[str, Any]]) -> None:
    for row in rows:
        ch1 = row.get("ecg_ch1")
        ch2 = row.get("ecg_ch2")
        if isinstance(ch1, (int, float)) and isinstance(ch2, (int, float)):
            row["ecg_diff_ch1_ch2"] = ch1 - ch2


def build_summary(input_path: pathlib.Path, rows: list[dict[str, Any]]) -> dict[str, Any]:
    ms_values = [row["ms"] for row in rows if isinstance(row.get("ms"), (int, float))]
    ecg_seq_values = [row["ecg_seq"] for row in rows if isinstance(row.get("ecg_seq"), (int, float))]
    sample_deltas = [b - a for a, b in zip(ms_values, ms_values[1:])]
    duration_ms = (ms_values[-1] - ms_values[0]) if len(ms_values) > 1 else 0
    rate_hz = (len(ms_values) * 1000.0 / duration_ms) if duration_ms else 0.0
    summary: dict[str, Any] = {
        "source": input_path.name,
        "rows": len(rows),
        "duration_s": round(duration_ms / 1000.0, 2),
        "rate_hz": round(rate_hz, 2),
    }
    if sample_deltas:
        summary.update(
            {
                "dt_avg_ms": round(sum(sample_deltas) / len(sample_deltas), 2),
                "dt_max_ms": max(sample_deltas),
            }
        )
    if len(ecg_seq_values) > 1:
        summary["ecg_seq_delta"] = ecg_seq_values[-1] - ecg_seq_values[0]
    return summary


def write_html(output_path: pathlib.Path, source_path: pathlib.Path, rows: list[dict[str, Any]]) -> None:
    payload = {
        "summary": build_summary(source_path, rows),
        "rows": rows,
        "panels": [
            {
                "id": "mic",
                "title": "MIC heart sound",
                "channels": ["mic_trace", "beat_envelope", "beat_threshold"],
                "note": "Mic waveform, filtered envelope and current threshold.",
            },
            {
                "id": "ecg",
                "title": "ECG clean view",
                "channels": ["ecg_diff_ch1_ch2"],
                "note": "Display-friendly CH1 - CH2 ECG trace.",
            },
            {
                "id": "acc",
                "title": "Accelerometer",
                "channels": ["acc_x_g", "acc_y_g", "acc_z_g"],
                "note": "X, Y and Z acceleration in g.",
            },
            {
                "id": "rate",
                "title": "BPM and motion",
                "channels": ["bpm", "motion_level"],
                "note": "Beat estimate and motion level.",
            },
            {
                "id": "raw",
                "title": "Raw ECG channels",
                "channels": ["ecg_ch1", "ecg_ch2", "ecg_ch3", "ecg_ch4"],
                "note": "Raw ADS1294 channels for debugging. Hidden by default.",
                "collapsed": True,
            },
        ],
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
:root { color-scheme: dark; --bg:#0b0f14; --panel:#121820; --text:#e6edf3; --muted:#8b949e; --line:#30363d; }
* { box-sizing: border-box; }
body { margin:0; background:var(--bg); color:var(--text); font-family:"Segoe UI", Arial, sans-serif; }
header { position:sticky; top:0; z-index:3; padding:14px 18px; background:#0b0f14; border-bottom:1px solid var(--line); }
h1 { margin:0 0 10px; font-size:20px; }
.summary { display:flex; flex-wrap:wrap; gap:8px; font-size:13px; color:var(--muted); }
.pill { border:1px solid var(--line); border-radius:999px; padding:4px 9px; background:#0f1620; }
.toolbar { display:flex; flex-wrap:wrap; align-items:center; gap:8px; padding:12px 18px 0; }
button { border:0; border-radius:6px; padding:7px 11px; background:#238636; color:white; cursor:pointer; }
button.secondary { background:#30363d; }
.file-button { display:inline-flex; align-items:center; border-radius:6px; padding:7px 11px; background:#1f6feb; color:white; cursor:pointer; font-size:13px; }
.file-button input { display:none; }
.file-status { color:var(--muted); font-size:13px; }
main { padding:12px 18px 24px; display:grid; gap:12px; }
.panel { background:var(--panel); border:1px solid var(--line); border-radius:8px; overflow:hidden; }
.panel-head { display:flex; align-items:center; justify-content:space-between; gap:10px; padding:10px 12px; border-bottom:1px solid var(--line); }
.panel h2 { margin:0; font-size:15px; }
.note { color:var(--muted); font-size:12px; }
.panel-body { padding:10px 12px 12px; }
canvas { width:100%; height:210px; display:block; background:#05080c; border:1px solid #242b35; }
.legend { display:flex; flex-wrap:wrap; gap:12px; margin-top:8px; font-size:12px; color:#c9d1d9; }
.legend span { display:inline-flex; align-items:center; gap:6px; }
.swatch { width:18px; height:3px; border-radius:99px; display:inline-block; }
.readout { margin-top:7px; color:var(--muted); font-size:12px; min-height:18px; }
.collapsed .panel-body { display:none; }
@media (max-width: 760px) { canvas { height:180px; } header { position:static; } }
</style>
</head>
<body>
<header>
  <h1>MotemaSens CSV Plot</h1>
  <div id="summary" class="summary"></div>
</header>
<div class="toolbar">
  <label class="file-button">Browse CSV<input id="csvFile" type="file" accept=".csv,text/csv"></label>
  <button id="resetZoom">Reset zoom</button>
  <button id="showRaw" class="secondary">Show raw ECG</button>
  <span id="fileStatus" class="file-status">Embedded sample loaded</span>
</div>
<main id="panels"></main>
<script>
const DEFAULT_DATA = __PAYLOAD__;
let DATA = DEFAULT_DATA;
let rows = DATA.rows;
const colors = ["#58a6ff","#f78166","#7ee787","#d2a8ff","#f2cc60","#79c0ff"];
let fullStart = rows[0].ms;
let fullEnd = rows[rows.length - 1].ms;
let viewStart = fullStart;
let viewEnd = fullEnd;
let drag = null;

function fmt(value) {
  if (typeof value !== "number") return value ?? "";
  if (Math.abs(value) >= 1000) return value.toFixed(0);
  if (Math.abs(value) >= 10) return value.toFixed(2);
  return value.toFixed(4);
}

function updateSummary() {
  document.getElementById("summary").innerHTML = Object.entries(DATA.summary)
    .map(([key, value]) => `<span class="pill">${key}: ${value}</span>`).join("");
}

function parseValue(value) {
  const trimmed = String(value ?? "").trim();
  if (trimmed === "") return null;
  if (/^0x/i.test(trimmed)) {
    const parsedHex = Number.parseInt(trimmed, 16);
    if (Number.isFinite(parsedHex)) return parsedHex;
  }
  const parsed = Number(trimmed);
  return Number.isFinite(parsed) ? parsed : trimmed;
}

function parseCsvLine(line) {
  const out = [];
  let value = "";
  let quoted = false;
  for (let i = 0; i < line.length; i++) {
    const char = line[i];
    if (char === '"' && quoted && line[i + 1] === '"') {
      value += '"';
      i++;
    } else if (char === '"') {
      quoted = !quoted;
    } else if (char === "," && !quoted) {
      out.push(value);
      value = "";
    } else {
      value += char;
    }
  }
  out.push(value);
  return out;
}

function addDerivedChannels(nextRows) {
  nextRows.forEach(row => {
    if (typeof row.ecg_ch1 === "number" && typeof row.ecg_ch2 === "number") {
      row.ecg_diff_ch1_ch2 = row.ecg_ch1 - row.ecg_ch2;
    }
  });
}

function buildSummary(source, nextRows) {
  const msValues = nextRows.map(row => row.ms).filter(value => typeof value === "number");
  const ecgSeqValues = nextRows.map(row => row.ecg_seq).filter(value => typeof value === "number");
  const deltas = [];
  for (let i = 1; i < msValues.length; i++) deltas.push(msValues[i] - msValues[i - 1]);
  const durationMs = msValues.length > 1 ? msValues[msValues.length - 1] - msValues[0] : 0;
  const summary = {
    source,
    rows: nextRows.length,
    duration_s: Number((durationMs / 1000).toFixed(2)),
    rate_hz: durationMs ? Number((msValues.length * 1000 / durationMs).toFixed(2)) : 0,
  };
  if (deltas.length) {
    summary.dt_avg_ms = Number((deltas.reduce((a, b) => a + b, 0) / deltas.length).toFixed(2));
    summary.dt_max_ms = Math.max(...deltas);
  }
  if (ecgSeqValues.length > 1) {
    summary.ecg_seq_delta = ecgSeqValues[ecgSeqValues.length - 1] - ecgSeqValues[0];
  }
  return summary;
}

function parseLogCsv(text, source) {
  let header = null;
  const nextRows = [];
  text.split(/\r?\n/).forEach(line => {
    if (!line.trim()) return;
    const cells = parseCsvLine(line);
    if (cells[0] === "LOG_HEADER") {
      header = cells;
      return;
    }
    if (cells[0] !== "LOG" || !header) return;
    const row = {};
    header.forEach((key, index) => { row[key] = parseValue(cells[index]); });
    nextRows.push(row);
  });
  if (!header) throw new Error("No LOG_HEADER row found");
  if (!nextRows.length) throw new Error("No LOG rows found");
  addDerivedChannels(nextRows);
  return {
    summary: buildSummary(source, nextRows),
    rows: nextRows,
    panels: DATA.panels,
  };
}

function setDataset(nextData, statusText) {
  DATA = nextData;
  rows = DATA.rows;
  fullStart = rows[0].ms;
  fullEnd = rows[rows.length - 1].ms;
  viewStart = fullStart;
  viewEnd = fullEnd;
  updateSummary();
  document.getElementById("fileStatus").textContent = statusText;
  drawAll();
}

function availableChannels(panel) {
  return panel.channels.filter(channel => rows.some(row => typeof row[channel] === "number" && Number.isFinite(row[channel])));
}

function visibleRows() {
  return rows.filter(row => row.ms >= viewStart && row.ms <= viewEnd);
}

function channelRange(data, channel) {
  let min = Infinity, max = -Infinity;
  data.forEach(row => {
    const value = row[channel];
    if (typeof value === "number" && Number.isFinite(value)) {
      min = Math.min(min, value);
      max = Math.max(max, value);
    }
  });
  if (!Number.isFinite(min) || !Number.isFinite(max)) return [-1, 1];
  if (min === max) return [min - 1, max + 1];
  const pad = (max - min) * 0.12;
  return [min - pad, max + pad];
}

function medianDelta(data) {
  const deltas = [];
  for (let i = 1; i < data.length; i++) deltas.push(data[i].ms - data[i - 1].ms);
  deltas.sort((a, b) => a - b);
  return deltas.length ? deltas[Math.floor(deltas.length / 2)] : 10;
}

function createPanels() {
  const root = document.getElementById("panels");
  root.innerHTML = DATA.panels.map((panel, index) => `
    <section class="panel ${panel.collapsed ? "collapsed" : ""}" data-index="${index}">
      <div class="panel-head">
        <div><h2>${panel.title}</h2><div class="note">${panel.note}</div></div>
        <button class="secondary toggle">${panel.collapsed ? "Show" : "Hide"}</button>
      </div>
      <div class="panel-body">
        <canvas></canvas>
        <div class="legend"></div>
        <div class="readout">Move mouse over this graph to inspect values.</div>
      </div>
    </section>`).join("");
  document.querySelectorAll(".toggle").forEach(button => {
    button.addEventListener("click", () => {
      const panelElement = button.closest(".panel");
      panelElement.classList.toggle("collapsed");
      button.textContent = panelElement.classList.contains("collapsed") ? "Show" : "Hide";
      drawAll();
    });
  });
}

function resizeCanvas(canvas) {
  const rect = canvas.getBoundingClientRect();
  const scale = window.devicePixelRatio || 1;
  canvas.width = Math.max(320, Math.floor(rect.width * scale));
  canvas.height = Math.max(180, Math.floor(rect.height * scale));
  const ctx = canvas.getContext("2d");
  ctx.setTransform(scale, 0, 0, scale, 0, 0);
  return { ctx, width: rect.width, height: rect.height };
}

function drawPanel(panelElement) {
  if (panelElement.classList.contains("collapsed")) return;
  const panel = DATA.panels[Number(panelElement.dataset.index)];
  const canvas = panelElement.querySelector("canvas");
  const { ctx, width:w, height:h } = resizeCanvas(canvas);
  const left = 54, right = 14, top = 14, bottom = 26;
  const plotW = w - left - right, plotH = h - top - bottom;
  const data = visibleRows();
  const channels = availableChannels(panel);
  const gapLimit = Math.max(200, medianDelta(data) * 4);

  ctx.clearRect(0, 0, w, h);
  ctx.fillStyle = "#05080c";
  ctx.fillRect(0, 0, w, h);
  ctx.strokeStyle = "#27303a";
  ctx.lineWidth = 1;
  ctx.font = "12px Segoe UI, Arial";
  ctx.fillStyle = "#8b949e";

  for (let i = 0; i <= 8; i++) {
    const x = left + plotW * i / 8;
    ctx.beginPath(); ctx.moveTo(x, top); ctx.lineTo(x, top + plotH); ctx.stroke();
    const ms = viewStart + (viewEnd - viewStart) * i / 8;
    ctx.fillText(`${(ms / 1000).toFixed(2)}s`, x - 14, top + plotH + 20);
  }
  for (let i = 0; i <= 4; i++) {
    const y = top + plotH * i / 4;
    ctx.beginPath(); ctx.moveTo(left, y); ctx.lineTo(left + plotW, y); ctx.stroke();
  }

  const ranges = Object.fromEntries(channels.map(channel => [channel, channelRange(data, channel)]));
  channels.forEach((channel, index) => {
    const [min, max] = ranges[channel];
    const color = colors[index % colors.length];
    ctx.strokeStyle = color;
    ctx.lineWidth = channel.includes("threshold") ? 1 : 1.45;
    ctx.beginPath();
    let started = false;
    let previousMs = null;
    data.forEach(row => {
      const value = row[channel];
      if (typeof value !== "number" || !Number.isFinite(value)) return;
      const x = left + ((row.ms - viewStart) / (viewEnd - viewStart || 1)) * plotW;
      const y = top + (1 - (value - min) / (max - min || 1)) * plotH;
      if (!started || (previousMs !== null && row.ms - previousMs > gapLimit)) {
        ctx.moveTo(x, y);
        started = true;
      } else {
        ctx.lineTo(x, y);
      }
      previousMs = row.ms;
    });
    ctx.stroke();
  });

  panelElement.querySelector(".legend").innerHTML = channels.map((channel, index) => {
    const range = ranges[channel] || [0, 0];
    return `<span><i class="swatch" style="background:${colors[index % colors.length]}"></i>${channel}: ${fmt(range[0])} to ${fmt(range[1])}</span>`;
  }).join("");
}

function drawAll() {
  document.querySelectorAll(".panel").forEach(drawPanel);
}

function rowAt(canvas, clientX) {
  const rect = canvas.getBoundingClientRect();
  const left = 54, right = 14;
  const x = Math.max(left, Math.min(rect.width - right, clientX - rect.left));
  const ms = viewStart + ((x - left) / (rect.width - left - right)) * (viewEnd - viewStart);
  let best = rows[0], distance = Infinity;
  rows.forEach(row => {
    const nextDistance = Math.abs(row.ms - ms);
    if (nextDistance < distance) { best = row; distance = nextDistance; }
  });
  return best;
}

function attachMouse() {
  document.querySelectorAll("canvas").forEach(canvas => {
    canvas.addEventListener("mousemove", event => {
      if (drag) {
        const dx = event.clientX - drag.x;
        const span = drag.end - drag.start;
        const shift = -dx / canvas.getBoundingClientRect().width * span;
        viewStart = Math.max(fullStart, drag.start + shift);
        viewEnd = Math.min(fullEnd, drag.end + shift);
        if (viewEnd - viewStart < span) {
          if (viewStart === fullStart) viewEnd = fullStart + span;
          if (viewEnd === fullEnd) viewStart = fullEnd - span;
        }
        drawAll();
        return;
      }
      const panelElement = canvas.closest(".panel");
      const panel = DATA.panels[Number(panelElement.dataset.index)];
      const row = rowAt(canvas, event.clientX);
      const channels = availableChannels(panel);
      panelElement.querySelector(".readout").textContent =
        [`ms: ${row.ms}`].concat(channels.map(channel => `${channel}: ${fmt(row[channel])}`)).join("   ");
    });
    canvas.addEventListener("mousedown", event => { drag = { x:event.clientX, start:viewStart, end:viewEnd }; });
    canvas.addEventListener("wheel", event => {
      event.preventDefault();
      const row = rowAt(canvas, event.clientX);
      const span = viewEnd - viewStart;
      const nextSpan = Math.max(50, Math.min(fullEnd - fullStart, span * (event.deltaY > 0 ? 1.2 : 0.82)));
      viewStart = Math.max(fullStart, row.ms - nextSpan / 2);
      viewEnd = Math.min(fullEnd, row.ms + nextSpan / 2);
      if (viewEnd - viewStart < nextSpan) {
        if (viewStart === fullStart) viewEnd = fullStart + nextSpan;
        if (viewEnd === fullEnd) viewStart = fullEnd - nextSpan;
      }
      drawAll();
    }, { passive:false });
  });
  window.addEventListener("mouseup", () => { drag = null; });
}

document.getElementById("resetZoom").onclick = () => { viewStart = fullStart; viewEnd = fullEnd; drawAll(); };
document.getElementById("csvFile").addEventListener("change", async event => {
  const file = event.target.files && event.target.files[0];
  if (!file) return;
  try {
    const text = await file.text();
    setDataset(parseLogCsv(text, file.name), `Loaded ${file.name}`);
  } catch (error) {
    document.getElementById("fileStatus").textContent = `Could not load CSV: ${error.message}`;
  }
});
document.getElementById("showRaw").onclick = () => {
  const raw = document.querySelector('[data-index="4"]');
  raw.classList.toggle("collapsed");
  raw.querySelector(".toggle").textContent = raw.classList.contains("collapsed") ? "Show" : "Hide";
  document.getElementById("showRaw").textContent = raw.classList.contains("collapsed") ? "Show raw ECG" : "Hide raw ECG";
  drawAll();
};
window.addEventListener("resize", drawAll);
updateSummary();
createPanels();
attachMouse();
drawAll();
</script>
</body>
</html>
"""


def main() -> int:
    parser = argparse.ArgumentParser(description="Convert MotemaSens CSV logs to a clean interactive HTML plot.")
    parser.add_argument("csv", type=pathlib.Path, help="Input CSV downloaded from /stream")
    parser.add_argument("-o", "--out", type=pathlib.Path, help="Output HTML path")
    args = parser.parse_args()

    input_path = args.csv
    output_path = args.out or input_path.with_suffix(".html")
    _header, rows = read_log_csv(input_path)
    write_html(output_path, input_path, rows)
    print(f"wrote {output_path} with {len(rows)} rows")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
