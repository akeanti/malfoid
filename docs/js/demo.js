// The spellbook: draw with mouse or finger, get judged. While nobody is
// drawing, the wand demonstrates the spells one after another.
import { COLORS, Sparkles, drawWand, easeInOut, fitCanvas, glowStroke, hexToRgb, walk } from "./canvas.js";
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
const NUMERALS = ["I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI", "XII"];
const DEMO = { draw: 1.7, hold: 1.2, fade: 0.5, gap: 0.35 };
const DEMO_CYCLE = DEMO.draw + DEMO.hold + DEMO.fade + DEMO.gap;
const RESUME_AFTER_MS = 7000;

const pick = (list) => list[(Math.random() * list.length) | 0];
const $ = (id) => document.getElementById(id);
const firstPath = (id) => SPELLBOOK.templates.find((t) => t.spell === id).path;

export function initDemo({ reduceMotion }) {
  const surface = $("draw-surface");
  const canvas = $("draw-canvas");
  if (!surface || !canvas) return;
  const rec = new Recognizer().loadBuiltins();
  const spells = new Map(SPELLBOOK.spells.map((s) => [s.id, s]));
  const sparkles = new Sparkles(600);
  const label = $("attract-label");

  let stroke = [];         // the user's drawing, canvas px
  let drawing = false;
  let strokeColor = COLORS.silverHi;
  let fadeStart = 0;
  let overlay = null;      // matched template mapped onto the drawing
  let demo = null;         // { index, start }
  let resumeAt = 0;
  let visible = true;
  let needsFrame = false;

  const view = fitCanvas(canvas, () => request());
  $("meter").style.setProperty("--threshold", `${ACCEPT_SCORE * 100}%`);
  new IntersectionObserver(([e]) => { visible = e.isIntersecting; if (visible) request(); }).observe(surface);

  // ------------------------------------------------------------- demo loop --
  function startDemo(index = 0) {
    if (reduceMotion) return;
    demo = { index: index % SPELLBOOK.spells.length, start: performance.now() };
    stroke = [];
    overlay = null;
    request();
  }

  function stopDemo() {
    demo = null;
    label.style.opacity = "0";
  }

  function demoPoints(spell) {
    const path = firstPath(spell.id);
    const { cx, cy, kx, ky } = normalization(path);
    const size = Math.min(view.w, view.h) * 0.5;
    const ox = view.w * 0.46, oy = view.h * 0.5;
    return path.map(([x, y]) => [ox + (x - cx) * kx * size, oy - (y - cy) * ky * size]);
  }

  // ---------------------------------------------------------------- input --
  const pos = (e) => {
    const r = canvas.getBoundingClientRect();
    return [e.clientX - r.left, e.clientY - r.top];
  };
  surface.addEventListener("pointerdown", (e) => {
    if (e.button !== 0) return;
    try { surface.setPointerCapture(e.pointerId); } catch { /* synthetic or already-released pointer */ }
    stopDemo();
    drawing = true;
    stroke = [pos(e)];
    strokeColor = COLORS.silverHi;
    overlay = null;
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
    resumeAt = performance.now() + RESUME_AFTER_MS;
  };
  surface.addEventListener("pointerup", finish);
  surface.addEventListener("pointercancel", finish);
  $("clear-btn").addEventListener("click", () => {
    stroke = [];
    overlay = null;
    highlight(null);
    startDemo(0);
  });

  // ------------------------------------------------------------- judging --
  function judge() {
    const flipped = stroke.map(([x, y]) => [x, -y]);  // the recognizer is y-up
    const len = pathLength(flipped);
    if (stroke.length < 4 || len < Math.min(view.w, view.h) * 0.12) {
      verdict({ name: "Fizzle", fail: true, effect: "That stroke was too small to read.", score: 0, sass: pick(SASS.timid) });
      return fizzle();
    }
    const m = rec.classify(flipped);
    const ok = accepts(m);
    const ambiguous = !ok && m.score >= ACCEPT_SCORE && m.runnerUp && m.score - m.runnerUp.score < MIN_MARGIN;
    const runner = m.runnerUp ? spells.get(m.runnerUp.id) : null;
    const runnerText = runner ? `Runner-up: ${runner.name} · ${Math.round(m.runnerUp.score * 100)}%` : "";

    if (!ok) {
      const sass = ambiguous
        ? pick(SASS.ambiguous).replace("{a}", m.spell.name).replace("{b}", runner?.name ?? "something else")
        : pick(SASS.fail);
      verdict({ name: ambiguous ? "Ambiguous" : "Fizzle", fail: true, effect: `Closest: ${m.spell.name}, ${m.spell.gesture.toLowerCase()}.`, score: m.score, sass, runner: runnerText });
      return fizzle();
    }

    const sass = m.id === "malfoid" ? pick(SASS.signature) : m.score > 0.82 ? pick(SASS.flawless) : pick(SASS.fine);
    verdict({ name: m.spell.name, effect: m.spell.effect, score: m.score, sass, runner: runnerText, spell: m.spell });
    highlight(m.id);

    // Map the matched template back onto the drawing with the exact
    // normalization the recognizer applied to it.
    const { cx, cy, kx, ky } = normalization(resample(flipped));
    overlay = { pts: m.template.map(([x, y]) => [x / kx + cx, -(y / ky + cy)]), color: m.spell.color, start: performance.now() };
    strokeColor = m.spell.color;
    if (!reduceMotion) {
      for (const p of stroke.filter((_, i) => i % 3 === 0)) {
        sparkles.emit(p[0], p[1], { count: 1, speed: 70, colors: [m.spell.color, COLORS.silverHi], life: [0.5, 1.2] });
      }
    }
    fadeStart = performance.now() + 2800;
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

  function verdict({ name, effect, score, sass, runner = "", fail = false, spell = null }) {
    const nameEl = $("verdict-name");
    nameEl.textContent = name;
    nameEl.classList.toggle("fail", fail);
    nameEl.style.color = !fail && spell ? spell.color : "";
    $("verdict-glyph").innerHTML = spell ? glyphSvg(spell, firstPath(spell.id), "v") : fail ? FIZZLE_SVG : "";
    $("verdict-effect").textContent = effect;
    const pct = Math.round(score * 100);
    $("meter-fill").style.width = `${pct}%`;
    $("meter").setAttribute("aria-valuenow", String(pct));
    $("meter-value").textContent = `${pct}%`;
    $("sass").replaceChildren(sass, Object.assign(document.createElement("cite"), { textContent: "Malfoid" }));
    $("runner").textContent = runner;
  }

  // ----------------------------------------------------------------- deck --
  const deck = $("spell-grid");
  SPELLBOOK.spells.forEach((spell, i) => {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = "card";
    btn.dataset.id = spell.id;
    btn.style.setProperty("--spell", spell.color);
    btn.setAttribute("aria-label", `${spell.name}: ${spell.gesture}. ${spell.effect}. Show me how it's drawn.`);
    btn.innerHTML = `<span class="numeral">${NUMERALS[i]}</span>${glyphSvg(spell, firstPath(spell.id), "c")}<b>${spell.name}</b><span>${spell.gesture}</span>`;
    btn.addEventListener("click", () => {
      highlight(spell.id);
      verdict({ name: spell.name, effect: `${spell.gesture}. ${spell.effect}.`, score: 0, sass: "Watch closely. I won't show you twice.", spell });
      $("draw-hint").style.opacity = "0";
      if (reduceMotion) {
        stroke = demoPoints(spell);
        strokeColor = spell.color;
        fadeStart = performance.now() + 2500;
        request();
      } else {
        startDemo(i);
      }
      resumeAt = 0;
    });
    deck.append(btn);
  });

  function highlight(id) {
    deck.querySelectorAll(".card").forEach((el) => el.classList.toggle("active", el.dataset.id === id));
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

    if (!demo && !drawing && resumeAt && now > resumeAt && !stroke.length) {
      resumeAt = 0;
      $("draw-hint").style.opacity = "1";
      startDemo(0);
    }

    if (demo && visible) {
      animating = true;
      const elapsed = (now - demo.start) / 1000;
      const spell = SPELLBOOK.spells[demo.index];
      const pts = demoPoints(spell);
      const phase = elapsed % DEMO_CYCLE;
      if (elapsed >= DEMO_CYCLE) {
        demo = { index: (demo.index + 1) % SPELLBOOK.spells.length, start: now };
      } else {
        const progress = phase < DEMO.draw ? easeInOut(phase / DEMO.draw) : 1;
        const alpha = phase < DEMO.draw + DEMO.hold ? 1 : Math.max(0, 1 - (phase - DEMO.draw - DEMO.hold) / DEMO.fade);
        const { point, path } = walk(pts, progress);
        glowStroke(ctx, pts, { color: spell.color, glow: spell.color, width: 1.2, blur: 0, alpha: 0.18 * alpha, dash: [4, 7] });
        if (alpha > 0) glowStroke(ctx, path, { color: COLORS.silverHi, glow: spell.color, width: 3, alpha });
        if (phase < DEMO.draw) sparkles.emit(point[0], point[1], { count: 2, speed: 45, colors: [spell.color, COLORS.silverHi] });
        if (alpha > 0) drawWand(ctx, point, [w * 1.04, h * 1.32], h * 0.95, { glow: 22 });
        label.querySelector("b").textContent = spell.name;
        label.querySelector("span").textContent = spell.gesture;
        label.style.opacity = String(alpha);
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

    if (resumeAt) animating = true;
    sparkles.step(dt);
    sparkles.draw(ctx);
    if (animating) request();
  }

  if (reduceMotion) label.style.opacity = "0";
  else startDemo(0);
}

const FIZZLE_SVG = `<svg viewBox="-1 -1 2 2"><path d="M-0.45,-0.45 L0.45,0.45 M0.45,-0.45 L-0.45,0.45" stroke="${COLORS.danger}" stroke-width="0.12" stroke-linecap="round"/></svg>`;

function glyphSvg(spell, path, prefix) {
  const { cx, cy, kx, ky } = normalization(path);
  const pts = path.map(([x, y]) => [((x - cx) * kx).toFixed(3), (-(y - cy) * ky).toFixed(3)]);
  const id = `arrow-${prefix}-${spell.id}`;
  return `<svg viewBox="-0.8 -0.8 1.6 1.6" aria-hidden="true">
    <defs><marker id="${id}" viewBox="0 0 10 10" refX="6" refY="5" markerWidth="3.2" markerHeight="3.2" orient="auto"><path d="M0 0L10 5L0 10z" fill="${spell.color}"/></marker></defs>
    <polyline points="${pts.map((p) => p.join(",")).join(" ")}" fill="none" stroke="${spell.color}" stroke-width="0.075" stroke-linecap="round" stroke-linejoin="round" marker-end="url(#${id})"/>
    <circle cx="${pts[0][0]}" cy="${pts[0][1]}" r="0.08" fill="#f6f8fa"/>
  </svg>`;
}
