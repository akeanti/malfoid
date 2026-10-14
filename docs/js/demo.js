// The spellbook: draw with mouse or finger, get judged.
import { COLORS, Sparkles, easeInOut, fitCanvas, glowStroke, hexToRgb, walk } from "./canvas.js";
import { ACCEPT_SCORE, MIN_MARGIN, Recognizer, accepts, normalization, pathLength, resample } from "./recognizer.js";
import { SPELLBOOK } from "./spellbook.js";

const SASS = {
  flawless: ["Flawless. Obviously.", "Exquisite. I'd expect nothing less.", "Platinum-grade casting, darling.", "Now that is how it's done."],
  fine: ["Acceptable. Barely.", "Sloppy wrist, but I'll allow it.", "It'll do. Don't let it go to your head.", "Passable. Practise anyway."],
  ambiguous: ["Was that {a} or {b}? Commit, darling.", "{a}? {b}? Make up your mind."],
  fail: ["Was that a spell or a sneeze?", "Tragic. Again, with conviction.", "I don't do interpretive dance.", "Honestly. Draw it like you mean it."],
  timid: ["Too timid. I can't read whispers.", "Bigger. Bolder. Braver."],
  signature: ["Now you're speaking my language.", "My name, in your hand. Charming."],
};
const pick = (list) => list[(Math.random() * list.length) | 0];
const $ = (id) => document.getElementById(id);

export function initDemo({ reduceMotion }) {
  const surface = $("draw-surface");
  const canvas = $("draw-canvas");
  if (!surface || !canvas) return;
  const rec = new Recognizer().loadBuiltins();
  const spells = new Map(SPELLBOOK.spells.map((s) => [s.id, s]));
  const sparkles = new Sparkles(600);

  let stroke = [];        // current / last drawing, canvas px
  let drawing = false;
  let strokeColor = COLORS.silverHi;
  let fadeStart = 0;      // when the last stroke started fading
  let overlay = null;     // { pts, color, start }
  let ghost = null;       // { pts, color, start } - a "how to draw it" demonstration
  let needsFrame = false;

  const view = fitCanvas(canvas, () => request());
  $("meter").style.setProperty("--threshold", `${ACCEPT_SCORE * 100}%`);

  // ---------------------------------------------------------------- input --
  const pos = (e) => {
    const r = canvas.getBoundingClientRect();
    return [e.clientX - r.left, e.clientY - r.top];
  };
  surface.addEventListener("pointerdown", (e) => {
    if (e.button !== 0) return;
    try { surface.setPointerCapture(e.pointerId); } catch { /* synthetic or already-released pointer */ }
    drawing = true;
    stroke = [pos(e)];
    strokeColor = COLORS.silverHi;
    overlay = ghost = null;
    fadeStart = 0;
    $("draw-hint").style.opacity = "0";
    request();
  });
  surface.addEventListener("pointermove", (e) => {
    if (!drawing) return;
    const coalesced = e.getCoalescedEvents?.();
    const events = coalesced?.length ? coalesced : [e];
    for (const ev of events) {
      const p = pos(ev);
      const last = stroke[stroke.length - 1];
      if (Math.hypot(p[0] - last[0], p[1] - last[1]) >= 1.5) stroke.push(p);
    }
    const tip = stroke[stroke.length - 1];
    if (!reduceMotion) sparkles.emit(tip[0], tip[1], { count: 2, speed: 40, life: [0.3, 0.8] });
    request();
  });
  const finish = () => {
    if (!drawing) return;
    drawing = false;
    judge();
  };
  surface.addEventListener("pointerup", finish);
  surface.addEventListener("pointercancel", finish);
  $("clear-btn").addEventListener("click", () => {
    stroke = [];
    overlay = ghost = null;
    $("draw-hint").style.opacity = "1";
    document.querySelectorAll(".spell.active").forEach((el) => el.classList.remove("active"));
    request();
  });

  // ------------------------------------------------------------- judging --
  function judge() {
    const flipped = stroke.map(([x, y]) => [x, -y]);  // recognizer uses y-up
    const len = pathLength(flipped);
    if (stroke.length < 4 || len < Math.min(view.w, view.h) * 0.12) {
      verdict({ name: "Fizzle", fail: true, effect: "That stroke was too small to read.", score: 0, sass: pick(SASS.timid) });
      return fizzle();
    }
    const m = rec.classify(flipped);
    const ok = accepts(m);
    const ambiguous = !ok && m.score >= ACCEPT_SCORE && m.runnerUp && m.score - m.runnerUp.score < MIN_MARGIN;
    const runner = m.runnerUp ? spells.get(m.runnerUp.id) : null;
    const runnerText = runner ? `Runner-up: ${runner.name} (${Math.round(m.runnerUp.score * 100)}%)` : "";

    if (!ok) {
      const sass = ambiguous
        ? pick(SASS.ambiguous).replace("{a}", m.spell.name).replace("{b}", runner?.name ?? "something else")
        : pick(SASS.fail);
      verdict({ name: ambiguous ? "Ambiguous" : "Fizzle", fail: true, effect: `Closest: ${m.spell.name}, ${m.spell.gesture.toLowerCase()}.`, score: m.score, sass, runner: runnerText });
      return fizzle();
    }

    const sass = m.id === "malfoid" ? pick(SASS.signature) : m.score > 0.82 ? pick(SASS.flawless) : pick(SASS.fine);
    verdict({ name: m.spell.name, effect: m.spell.effect, score: m.score, sass, runner: runnerText, color: m.spell.color });
    highlight(m.id);

    // Map the matched template back onto the drawing, using the exact
    // normalization the recognizer applied to it.
    const { cx, cy, kx, ky } = normalization(resample(flipped));
    overlay = { pts: m.template.map(([x, y]) => [x / kx + cx, -(y / ky + cy)]), color: m.spell.color, start: performance.now() };
    strokeColor = m.spell.color;
    if (!reduceMotion) {
      for (const p of stroke.filter((_, i) => i % 3 === 0)) {
        sparkles.emit(p[0], p[1], { count: 1, speed: 70, colors: [m.spell.color, COLORS.silverHi], life: [0.5, 1.2] });
      }
    }
    fadeStart = performance.now() + 2600;
    request();
  }

  function fizzle() {
    strokeColor = COLORS.danger;
    if (!reduceMotion) {
      const tip = stroke[stroke.length - 1];
      if (tip) sparkles.emit(tip[0], tip[1], { count: 26, speed: 90, colors: [COLORS.danger, "#ffb3a6"], life: [0.3, 0.7] });
    }
    fadeStart = performance.now() + 900;
    request();
  }

  function verdict({ name, effect, score, sass, runner = "", fail = false, color = null }) {
    const nameEl = $("verdict-name");
    nameEl.textContent = name;
    nameEl.classList.toggle("fail", fail);
    nameEl.style.color = !fail && color ? color : "";
    $("verdict-effect").textContent = effect;
    const pct = Math.round(score * 100);
    $("meter-fill").style.width = `${pct}%`;
    $("meter").setAttribute("aria-valuenow", String(pct));
    $("meter-value").textContent = `${pct}%`;
    $("sass").innerHTML = "";
    $("sass").append(sass, Object.assign(document.createElement("cite"), { textContent: "Malfoid" }));
    $("runner").textContent = runner;
  }

  // ------------------------------------------------------------ spell grid --
  const grid = $("spell-grid");
  for (const spell of SPELLBOOK.spells) {
    const path = SPELLBOOK.templates.find((t) => t.spell === spell.id).path;
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = "spell";
    btn.dataset.id = spell.id;
    btn.style.setProperty("--spell", spell.color);
    btn.setAttribute("aria-label", `${spell.name}: ${spell.gesture}. ${spell.effect}. Show me how.`);
    btn.innerHTML = `${glyphSvg(spell, path)}<div><b>${spell.name}</b><span>${spell.gesture}</span><span>${spell.effect}</span></div>`;
    btn.addEventListener("click", () => demonstrate(spell, path));
    grid.append(btn);
  }

  function highlight(id) {
    document.querySelectorAll(".spell").forEach((el) => el.classList.toggle("active", el.dataset.id === id));
  }

  function demonstrate(spell, path) {
    const { cx, cy, kx, ky } = normalization(path);
    const size = Math.min(view.w, view.h) * 0.56;
    const pts = path.map(([x, y]) => [view.w / 2 + (x - cx) * kx * size, view.h / 2 - (y - cy) * ky * size]);
    stroke = [];
    overlay = null;
    ghost = { pts, color: spell.color, start: performance.now() };
    $("draw-hint").style.opacity = "0";
    highlight(spell.id);
    verdict({ name: spell.name, effect: `${spell.gesture}. ${spell.effect}.`, score: 0, sass: "Watch closely. I won't show you twice.", color: spell.color });
    request();
  }

  // ----------------------------------------------------------- rendering --
  function request() {
    if (needsFrame) return;
    needsFrame = true;
    requestAnimationFrame(frame);
  }

  let last = performance.now();
  function frame(now) {
    needsFrame = false;
    const dt = Math.min(0.05, (now - last) / 1000);
    last = now;
    const { ctx, w, h } = view;
    ctx.clearRect(0, 0, w, h);
    let animating = drawing || sparkles.alive;

    if (ghost) {
      const t = Math.min(1, (now - ghost.start) / 1600);
      const { point, path } = walk(ghost.pts, easeInOut(t));
      glowStroke(ctx, ghost.pts, { color: ghost.color, glow: ghost.color, width: 1.4, blur: 0, alpha: 0.25, dash: [4, 7] });
      glowStroke(ctx, path, { color: COLORS.silverHi, glow: ghost.color, width: 3 });
      ctx.beginPath();
      ctx.arc(ghost.pts[0][0], ghost.pts[0][1], 5, 0, Math.PI * 2);
      ctx.fillStyle = ghost.color;
      ctx.fill();
      if (t < 1) {
        animating = true;
        if (!reduceMotion) sparkles.emit(point[0], point[1], { count: 2, speed: 40, colors: [ghost.color, COLORS.silverHi] });
      }
    }

    if (stroke.length > 1) {
      let alpha = 1;
      if (fadeStart && now > fadeStart) alpha = Math.max(0, 1 - (now - fadeStart) / 900);
      if (alpha > 0) {
        glowStroke(ctx, stroke, { color: strokeColor === COLORS.silverHi ? COLORS.silverHi : "#ffffff", glow: strokeColor, width: 3, alpha });
        if (overlay) {
          const t = Math.min(1, (now - overlay.start) / 650);
          const [r, g, b] = hexToRgb(overlay.color);
          glowStroke(ctx, walk(overlay.pts, easeInOut(t)).path, { color: `rgba(${r},${g},${b},0.9)`, glow: overlay.color, width: 1.6, blur: 8, alpha: alpha * 0.9, dash: [6, 6] });
          if (t < 1) animating = true;
        }
        if (fadeStart) animating = true;
      } else {
        stroke = [];
        overlay = null;
      }
    }

    sparkles.step(dt);
    sparkles.draw(ctx);
    if (animating) request();
  }
}

function glyphSvg(spell, path) {
  const { cx, cy, kx, ky } = normalization(path);
  const pts = path.map(([x, y]) => [((x - cx) * kx).toFixed(3), (-(y - cy) * ky).toFixed(3)]);
  const id = `arrow-${spell.id}`;
  return `<svg viewBox="-0.78 -0.78 1.56 1.56" aria-hidden="true">
    <defs><marker id="${id}" viewBox="0 0 10 10" refX="6" refY="5" markerWidth="3.2" markerHeight="3.2" orient="auto"><path d="M0 0L10 5L0 10z" fill="${spell.color}"/></marker></defs>
    <polyline points="${pts.map((p) => p.join(",")).join(" ")}" fill="none" stroke="${spell.color}" stroke-width="0.075" stroke-linecap="round" stroke-linejoin="round" marker-end="url(#${id})"/>
    <circle cx="${pts[0][0]}" cy="${pts[0][1]}" r="0.08" fill="#f4f7f9"/>
  </svg>`;
}
