// Web Serial console for a USB-connected wand. Speaks the newline-delimited
// JSON protocol documented in firmware/src/console.h.
import { COLORS, fitCanvas, glowStroke } from "./canvas.js";

const $ = (id) => document.getElementById(id);
const MAX_LOG = 200;
const TRAIL = 120;

export function initConsole() {
  const root = $("console-root");
  if (!root) return;
  const connectBtn = $("connect-btn");
  const logEl = $("log");
  const form = $("p-settings");
  const sky = fitCanvas($("sky-canvas"), () => drawSky());

  let port = null, reader = null, writer = null, readable = null;
  let home = null;          // azimuth that maps to the centre of the sky view
  let trail = [];           // [{x, y, casting}]
  let lastCast = null;      // { pts, ok, color }
  let device = null;        // last hello payload
  let wasCasting = false;

  if (!("serial" in navigator)) {
    $("serial-unsupported").hidden = false;
    connectBtn.disabled = true;
    log("Web Serial isn't available in this browser.", "err");
    drawSky();
    return;
  }

  // ------------------------------------------------------------ transport --
  connectBtn.addEventListener("click", () => (port ? disconnect() : connect()));
  navigator.serial.addEventListener("disconnect", (e) => {
    if (e.target === port) {
      log("Wand disconnected (asleep or unplugged).");
      teardown();
    }
  });

  async function connect() {
    try {
      port = await navigator.serial.requestPort();
      await port.open({ baudRate: 115200 });
    } catch (err) {
      if (err.name !== "NotFoundError") log(`Could not open the port: ${err.message}`, "err");
      port = null;
      return;
    }
    writer = port.writable.getWriter();
    setConnected(true);
    log("Port open. Saying hello…");
    readLoop();
    await send({ cmd: "hello" });
    await send({ cmd: "stream", on: true });
  }

  async function readLoop() {
    const decoder = new TextDecoderStream();
    readable = port.readable.pipeTo(decoder.writable).catch(() => {});
    reader = decoder.readable.getReader();
    let buffer = "";
    try {
      for (;;) {
        const { value, done } = await reader.read();
        if (done) break;
        buffer += value;
        let nl;
        while ((nl = buffer.indexOf("\n")) >= 0) {
          const line = buffer.slice(0, nl).trim();
          buffer = buffer.slice(nl + 1);
          if (line) handleLine(line);
        }
        if (buffer.length > 8192) buffer = "";  // not our protocol; don't grow forever
      }
    } catch (err) {
      log(`Read error: ${err.message}`, "err");
    } finally {
      reader.releaseLock();
    }
  }

  async function disconnect() {
    try { await reader?.cancel(); } catch { /* already closed */ }
    try { await readable; } catch { /* ignore */ }
    try { writer?.releaseLock(); } catch { /* ignore */ }
    try { await port?.close(); } catch { /* ignore */ }
    log("Disconnected.");
    teardown();
  }

  function teardown() {
    port = reader = writer = readable = null;
    device = null;
    setConnected(false);
  }

  async function send(obj) {
    if (!writer) return;
    const line = JSON.stringify(obj);
    log(`→ ${obj.cmd === "set" ? redact(obj) : line}`, "in");
    await writer.write(new TextEncoder().encode(line + "\n"));
  }

  const redact = (obj) => JSON.stringify(obj, (k, v) => (k.endsWith("_pass") && v ? "••••" : v));

  // -------------------------------------------------------------- protocol --
  function handleLine(line) {
    let msg;
    try { msg = JSON.parse(line); } catch { return log(line); }  // boot chatter, panics, etc.
    switch (msg.evt) {
      case "hello": return onHello(msg);
      case "status": return onStatus(msg);
      case "live": return onLive(msg);
      case "cast": return onCast(msg);
      case "armed": return log(`Armed: cast once to record "${msg.name}".`);
      case "recorded":
        lastCast = { pts: msg.pts, ok: true, color: COLORS.silverHi };
        castLabel(`${msg.name} learned`, `${msg.pts.length} points stored`);
        log(`Recorded "${msg.name}" (${msg.id}).`);
        send({ cmd: "hello" });
        return;
      case "sleep": return log(`She's going to sleep (${msg.why}). Pick her up to wake her.`);
      case "ok": return msg.cmd === "stream" ? undefined : log(`✓ ${msg.cmd}`);
      case "error": return log(`✗ ${msg.cmd || "error"}: ${msg.msg}`, "err");
      default: return log(line);
    }
  }

  function onHello(msg) {
    device = msg;
    const hw = msg.hw || {};
    const missing = ["imu", "haptics", "gauge"].filter((k) => hw[k] === false);
    $("device-line").textContent =
      `${msg.settings?.name ?? "Malfoid"} · ${msg.board} · firmware ${msg.fw} · id ${msg.id}` +
      (missing.length ? ` · not found: ${missing.join(", ")}` : "");
    if (missing.length) log(`Hardware not detected: ${missing.join(", ")}. Check wiring and I²C addresses.`, "err");

    const s = msg.settings || {};
    for (const [key, value] of Object.entries(s)) {
      const field = form.elements.namedItem(key);
      if (field) field.value = value;
    }
    for (const key of ["wifi_pass", "mqtt_pass"]) {
      const field = form.elements.namedItem(key);
      field.value = "";
      field.placeholder = s[`${key}_set`] ? "saved - leave blank to keep" : "";
    }

    const select = $("test-select");
    select.innerHTML = "";
    const list = $("custom-list");
    list.innerHTML = "";
    const seen = new Set();
    for (const spell of msg.spells || []) {
      if (!seen.has(spell.id)) {
        seen.add(spell.id);
        select.append(new Option(spell.name, spell.id));
      }
      if (spell.custom) {
        const li = document.createElement("li");
        li.append(Object.assign(document.createElement("span"), { textContent: `${spell.name} (${spell.id})` }));
        const forget = Object.assign(document.createElement("button"), { type: "button", className: "btn btn-sm", textContent: "Forget" });
        forget.addEventListener("click", () => send({ cmd: "forget", id: spell.id }));
        li.append(forget);
        list.append(li);
      }
    }
    if (!list.children.length) list.innerHTML = '<li class="empty">No recorded spells yet.</li>';
    log(`Hello from ${msg.settings?.name ?? "Malfoid"} (${msg.board}, fw ${msg.fw}).`);
  }

  function onStatus(msg) {
    chip("chip-wifi", msg.wifi, msg.wifi ? `Wi-Fi ${msg.rssi} dBm` : "Wi-Fi");
    chip("chip-mqtt", msg.mqtt, "MQTT");
    if (msg.bat !== undefined) {
      chip("chip-bat", true, `${Math.round(msg.bat)}%${msg.chg ? " ⚡" : ""}`, msg.bat < 15);
    } else {
      chip("chip-bat", false, "Battery");
    }
  }

  function onLive(msg) {
    if (home === null || (msg.casting && !wasCasting)) {
      home = msg.az;
      if (msg.casting) trail = [];
    }
    wasCasting = msg.casting;
    let dx = -(msg.az - home);
    dx = ((dx + 540) % 360) - 180;
    trail.push({ x: dx, y: msg.el, casting: msg.casting });
    if (trail.length > TRAIL) trail.shift();
    $("sky-readout").textContent =
      `az ${msg.az.toFixed(1)}°  el ${msg.el.toFixed(1)}°\n` +
      `${msg.casting ? "casting…" : msg.held ? "thumb on gem" : "idle"}${msg.bias_ok ? "" : " · learning gyro bias (hold still)"}`;
    drawSky();
  }

  function onCast(msg) {
    if (!msg.ok && msg.reason) {
      castLabel("Fizzle", msg.reason.replace("_", " "));
      log(`Cast rejected: ${msg.reason.replace("_", " ")} (${msg.ms} ms, ${msg.sweep}° sweep).`);
      return;
    }
    lastCast = { pts: msg.pts, ok: msg.ok, color: msg.ok ? COLORS.emerald : COLORS.danger };
    const pct = Math.round(msg.score * 100);
    castLabel(msg.ok ? msg.name : "Fizzle", `${pct}% · ${msg.ms} ms · ${msg.sweep}°`);
    log(`${msg.ok ? "✦" : "✗"} ${msg.name} ${pct}%${msg.runner ? ` (runner-up ${msg.runner} ${Math.round(msg.runner_score * 100)}%)` : ""}`);
    drawSky();
  }

  // --------------------------------------------------------------- forms --
  form.addEventListener("submit", (e) => {
    e.preventDefault();
    const out = { cmd: "set" };
    for (const el of form.elements) {
      if (!el.name) continue;
      const v = el.value.trim();
      if (el.type === "password") { if (v) out[el.name] = v; continue; }
      if (el.type === "number") { if (v !== "") out[el.name] = Number(v); continue; }
      out[el.name] = v;
    }
    send(out);
  });
  $("record-form").addEventListener("submit", (e) => {
    e.preventDefault();
    const name = e.target.elements.namedItem("name").value.trim();
    if (name) send({ cmd: "record", name });
  });
  $("test-btn").addEventListener("click", () => send({ cmd: "test", id: $("test-select").value }));
  $("sleep-btn").addEventListener("click", () => send({ cmd: "sleep" }));

  // ------------------------------------------------------------------ UI --
  function setConnected(on) {
    root.classList.toggle("offline", !on);
    connectBtn.textContent = on ? "Disconnect" : "Connect wand";
    chip("chip-usb", on, "USB");
    if (!on) {
      for (const id of ["chip-wifi", "chip-mqtt", "chip-bat"]) chip(id, false, $(id).dataset.label || $(id).textContent);
      $("sky-readout").textContent = "not connected";
      $("device-line").textContent = "No wand connected.";
      home = null;
      trail = [];
    }
    drawSky();
  }

  function chip(id, on, text, warn = false) {
    const el = $(id);
    el.dataset.label ??= el.textContent;
    el.classList.toggle("on", Boolean(on));
    el.classList.toggle("warn", warn);
    el.textContent = on ? text : el.dataset.label;
  }

  function castLabel(name, detail) {
    $("sky-cast-name").textContent = name;
    $("sky-cast-detail").textContent = detail;
  }

  function log(text, cls = "") {
    const line = document.createElement("div");
    if (cls) line.className = cls;
    line.textContent = `${new Date().toLocaleTimeString([], { hour12: false })}  ${text}`;
    logEl.append(line);
    while (logEl.children.length > MAX_LOG) logEl.firstChild.remove();
    logEl.scrollTop = logEl.scrollHeight;
  }

  function drawSky() {
    const { ctx, w, h } = sky;
    ctx.clearRect(0, 0, w, h);
    const sx = (x) => w / 2 + (x / 100) * (w / 2);   // +/-100 degrees across
    const sy = (y) => h / 2 - (y / 70) * (h / 2);    // +/-70 degrees up/down

    ctx.strokeStyle = "rgba(201,209,217,0.07)";
    ctx.lineWidth = 1;
    for (let a = -90; a <= 90; a += 30) {
      ctx.beginPath(); ctx.moveTo(sx(a), 0); ctx.lineTo(sx(a), h); ctx.stroke();
    }
    for (let e = -60; e <= 60; e += 30) {
      ctx.beginPath(); ctx.moveTo(0, sy(e)); ctx.lineTo(w, sy(e)); ctx.stroke();
    }
    ctx.strokeStyle = "rgba(47,211,138,0.25)";
    ctx.beginPath(); ctx.moveTo(0, sy(0)); ctx.lineTo(w, sy(0)); ctx.stroke();

    if (trail.length > 1) {
      const pts = trail.map((p) => [sx(p.x), sy(p.y)]);
      glowStroke(ctx, pts, { color: "rgba(201,209,217,0.35)", glow: "rgba(201,209,217,0.2)", width: 1.2, blur: 0 });
      const hot = [];
      for (let i = 0; i < trail.length; i++) if (trail[i].casting) hot.push(pts[i]);
      if (hot.length > 1) glowStroke(ctx, hot, { color: COLORS.silverHi, glow: COLORS.emerald, width: 2.6 });
      const tip = pts[pts.length - 1];
      const g = ctx.createRadialGradient(tip[0], tip[1], 0, tip[0], tip[1], 16);
      g.addColorStop(0, "rgba(255,255,255,0.95)");
      g.addColorStop(1, "rgba(47,211,138,0)");
      ctx.fillStyle = g;
      ctx.beginPath(); ctx.arc(tip[0], tip[1], 16, 0, Math.PI * 2); ctx.fill();
    }

    if (lastCast?.pts?.length) {  // the normalized stroke, in an inset
      const box = Math.min(110, h * 0.42), x0 = w - box - 14, y0 = h - box - 14;
      ctx.fillStyle = "rgba(10,22,17,0.85)";
      ctx.strokeStyle = "rgba(201,209,217,0.15)";
      ctx.beginPath(); ctx.roundRect(x0, y0, box, box, 10); ctx.fill(); ctx.stroke();
      const pts = lastCast.pts.map(([x, y]) => [x0 + box / 2 + x * box * 0.72, y0 + box / 2 - y * box * 0.72]);
      glowStroke(ctx, pts, { color: COLORS.silverHi, glow: lastCast.color, width: 2 });
    }
  }
}
