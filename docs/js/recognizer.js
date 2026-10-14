// JavaScript twin of firmware/lib/malfoid_core (stroke.cpp + recognizer.cpp).
// Keep the two in lockstep: same resampling, same normalization, same DTW band,
// same score curve - so a gesture that works in the browser works on the wand.

import { SPELLBOOK } from "./spellbook.js";

export const STROKE_POINTS = SPELLBOOK.points;
export const DEFAULT_WINDOW = 4;
export const ZERO_SCORE_DISTANCE = 0.35;
export const ACCEPT_SCORE = 0.55;
export const MIN_MARGIN = 0.05;
export const ONE_D_RATIO = 0.2;

// Same rule as Recognizer::accepts() in the firmware.
export const accepts = (m, minScore = ACCEPT_SCORE, minMargin = MIN_MARGIN) =>
  Boolean(m) && m.score >= minScore && m.score - (m.runnerUp?.score ?? 0) >= minMargin;

const dist = (a, b) => Math.hypot(a[0] - b[0], a[1] - b[1]);

export function pathLength(pts) {
  let total = 0;
  for (let i = 1; i < pts.length; i++) total += dist(pts[i - 1], pts[i]);
  return total;
}

export function resample(pts, n = STROKE_POINTS) {
  if (pts.length < 2) return null;
  const total = pathLength(pts);
  if (total <= 1e-6) return null;
  const step = total / (n - 1);
  const out = [[pts[0][0], pts[0][1]]];
  let carried = 0;
  let prev = pts[0];
  for (let i = 1; i < pts.length && out.length < n; i++) {
    const cur = pts[i];
    let seg = dist(prev, cur);
    while (seg > 0 && carried + seg >= step && out.length < n) {
      const t = (step - carried) / seg;
      const q = [prev[0] + (cur[0] - prev[0]) * t, prev[1] + (cur[1] - prev[1]) * t];
      out.push(q);
      prev = q;
      seg = dist(prev, cur);
      carried = 0;
    }
    carried += seg;
    prev = cur;
  }
  while (out.length < n) out.push([pts[pts.length - 1][0], pts[pts.length - 1][1]]);
  return out;
}

// sqrt(minor / major) principal-axis variance: ~0 for a line at any tilt, ~1 for a circle.
export function thinness(pts, cx, cy) {
  let sxx = 0, syy = 0, sxy = 0;
  for (const [x, y] of pts) {
    const dx = x - cx, dy = y - cy;
    sxx += dx * dx; syy += dy * dy; sxy += dx * dy;
  }
  const mean = (sxx + syy) / 2;
  const spread = Math.sqrt(((sxx - syy) / 2) ** 2 + sxy ** 2);
  const major = mean + spread, minor = Math.max(0, mean - spread);
  return major > 0 ? Math.sqrt(minor / major) : 0;
}

// The translation and per-axis scale normalize() applies (exposed so the demo
// can map a template back onto the user's drawing).
export function normalization(pts) {
  let cx = 0, cy = 0;
  let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
  for (const [x, y] of pts) {
    cx += x; cy += y;
    minX = Math.min(minX, x); maxX = Math.max(maxX, x);
    minY = Math.min(minY, y); maxY = Math.max(maxY, y);
  }
  cx /= pts.length; cy /= pts.length;
  const w = maxX - minX, h = maxY - minY;
  let size = Math.max(w, h);
  if (size < 1e-6) size = 1;
  // Thin, line-like strokes keep their aspect ratio; 2-D shapes are stretched
  // to a square so a wide M still looks like an M.
  const oneD = w < 1e-6 || h < 1e-6 || thinness(pts, cx, cy) < ONE_D_RATIO;
  return { cx, cy, kx: oneD ? 1 / size : 1 / w, ky: oneD ? 1 / size : 1 / h, oneD };
}

export function normalize(pts) {
  const { cx, cy, kx, ky } = normalization(pts);
  return pts.map(([x, y]) => [(x - cx) * kx, (y - cy) * ky]);
}

export function prepare(pts) {
  const r = resample(pts);
  return r ? normalize(r) : null;
}

export function dtw(a, b, window = DEFAULT_WINDOW) {
  const n = a.length;
  let prev = new Float64Array(n + 1).fill(Infinity);
  let cur = new Float64Array(n + 1);
  prev[0] = 0;
  for (let i = 1; i <= n; i++) {
    cur[0] = Infinity;
    for (let j = 1; j <= n; j++) {
      if (Math.abs(i - j) > window) { cur[j] = Infinity; continue; }
      cur[j] = dist(a[i - 1], b[j - 1]) + Math.min(prev[j - 1], prev[j], cur[j - 1]);
    }
    [prev, cur] = [cur, prev];
  }
  return prev[n] / n;
}

export const score = (d) => Math.max(0, Math.min(1, 1 - d / ZERO_SCORE_DISTANCE));

export class Recognizer {
  constructor({ window = DEFAULT_WINDOW } = {}) {
    this.window = window;
    this.templates = [];
    this.spells = new Map(SPELLBOOK.spells.map((s) => [s.id, s]));
  }

  loadBuiltins() {
    for (const t of SPELLBOOK.templates) this.addPath(t.spell, t.path, false);
    return this;
  }

  addPath(id, path, custom = true) {
    const stroke = prepare(path);
    if (stroke) this.templates.push({ id, stroke, custom });
    return Boolean(stroke);
  }

  // Returns { id, spell, score, distance, template, runnerUp } or null with no templates.
  classify(rawPath) {
    const stroke = prepare(rawPath);
    if (!stroke || !this.templates.length) return null;
    const scored = this.templates.map((t) => ({ t, d: dtw(stroke, t.stroke, this.window) }));
    scored.sort((x, y) => x.d - y.d);
    const best = scored[0];
    const runner = scored.find((s) => s.t.id !== best.t.id);
    return {
      id: best.t.id,
      spell: this.spells.get(best.t.id) ?? { id: best.t.id, name: best.t.id, effect: "Custom spell" },
      distance: best.d,
      score: score(best.d),
      stroke,
      template: best.t.stroke,
      runnerUp: runner ? { id: runner.t.id, distance: runner.d, score: score(runner.d) } : null,
    };
  }
}
