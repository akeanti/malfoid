// Monte-Carlo tuning for the recognizer: synthesizes sloppy casts of every spell
// plus random scribbles, then reports accuracy, false accepts and the distance
// distribution so ZERO_SCORE_DISTANCE / ACCEPT_SCORE can be chosen with data.
//
//   node tools/tune_recognizer.mjs [casts-per-spell] [--assert]
//
// --assert exits non-zero if accuracy regresses (used by CI).

import { Recognizer, ACCEPT_SCORE, MIN_MARGIN, ZERO_SCORE_DISTANCE, accepts } from "../docs/js/recognizer.js";
import { SPELLBOOK } from "../docs/js/spellbook.js";

const args = process.argv.slice(2);
const perSpell = Number(args.find((a) => /^\d+$/.test(a)) ?? 300);
const assertMode = args.includes("--assert");
let seed = 0x4d414c46; // "MALF"
const rand = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 2 ** 32);
const between = (a, b) => a + (b - a) * rand();

function densify(path, n = 120) {
  const out = [];
  const segs = path.length - 1;
  for (let i = 0; i < n; i++) {
    // non-uniform time: people speed up mid-stroke
    const u = i / (n - 1);
    const t = Math.min(1, Math.max(0, u + 0.12 * Math.sin(2 * Math.PI * u) * between(0.2, 1)));
    const f = t * segs;
    const k = Math.min(segs - 1, Math.floor(f));
    const w = f - k;
    out.push([path[k][0] + (path[k + 1][0] - path[k][0]) * w, path[k][1] + (path[k + 1][1] - path[k][1]) * w]);
  }
  return out;
}

function sloppy(path) {
  const rot = between(-0.3, 0.3); // +/- 17 degrees
  const sx = between(0.7, 1.4), sy = between(0.7, 1.4), shear = between(-0.2, 0.2);
  const wobA = between(0, 0.06), wobF = between(1, 3), wobP = between(0, 6.28);
  let pts = densify(path).map(([x, y], i, arr) => {
    const u = i / (arr.length - 1);
    let X = x * sx + y * shear, Y = y * sy;
    [X, Y] = [X * Math.cos(rot) - Y * Math.sin(rot), X * Math.sin(rot) + Y * Math.cos(rot)];
    X += wobA * Math.sin(2 * Math.PI * wobF * u + wobP) + between(-0.012, 0.012);
    Y += wobA * Math.cos(2 * Math.PI * wobF * u + wobP) + between(-0.012, 0.012);
    return [X, Y];
  });
  // trim a bit of either end (late start / early release)
  const a = Math.floor(pts.length * between(0, 0.06));
  const b = pts.length - Math.floor(pts.length * between(0, 0.06));
  return pts.slice(a, b);
}

function scribble() {
  const pts = [[0, 0]];
  let ang = between(0, 6.28);
  const steps = 20 + Math.floor(rand() * 60);
  for (let i = 0; i < steps; i++) {
    ang += between(-1.2, 1.2);
    const [x, y] = pts[pts.length - 1];
    pts.push([x + Math.cos(ang) * 0.05, y + Math.sin(ang) * 0.05]);
  }
  return pts;
}

const rec = new Recognizer().loadBuiltins();
const confusion = {};
const genuine = [];
const samples = [];
const junkSamples = [];
let correct = 0, accepted = 0, wrongAccepted = 0, total = 0;
const sourceOf = new Map();
for (const t of SPELLBOOK.templates) if (!sourceOf.has(t.spell)) sourceOf.set(t.spell, []);
for (const t of SPELLBOOK.templates) sourceOf.get(t.spell).push(t.path);

for (const spell of SPELLBOOK.spells) {
  const variants = sourceOf.get(spell.id);
  for (let i = 0; i < perSpell; i++) {
    const m = rec.classify(sloppy(variants[i % variants.length]));
    total++;
    genuine.push(m.distance);
    const ok = m.id === spell.id;
    samples.push({ s: m.score, r: m.runnerUp?.score ?? 0, ok });
    if (ok) correct++;
    if (accepts(m)) {
      if (ok) accepted++;
      else wrongAccepted++;
    }
    if (!ok) confusion[`${spell.id} -> ${m.id}`] = (confusion[`${spell.id} -> ${m.id}`] ?? 0) + 1;
  }
}

const junk = [];
let junkAccepted = 0;
const junkN = perSpell * 3;
for (let i = 0; i < junkN; i++) {
  const m = rec.classify(scribble());
  junk.push(m.distance);
  junkSamples.push({ s: m.score, r: m.runnerUp?.score ?? 0 });
  if (accepts(m)) junkAccepted++;
}

const pct = (x, n) => `${((100 * x) / n).toFixed(1)}%`;
const quant = (arr, q) => [...arr].sort((a, b) => a - b)[Math.floor(q * (arr.length - 1))].toFixed(3);
console.log(`ZERO_SCORE_DISTANCE=${ZERO_SCORE_DISTANCE} ACCEPT_SCORE=${ACCEPT_SCORE} MIN_MARGIN=${MIN_MARGIN} -> accept if d <= ${(ZERO_SCORE_DISTANCE * (1 - ACCEPT_SCORE)).toFixed(3)}`);
console.log(`genuine casts: ${total}, top-1 correct ${pct(correct, total)}, accepted+correct ${pct(accepted, total)}, accepted+WRONG ${pct(wrongAccepted, total)}`);
console.log(`genuine distance p50 ${quant(genuine, 0.5)}  p90 ${quant(genuine, 0.9)}  p99 ${quant(genuine, 0.99)}`);
console.log(`scribbles: ${junkN}, falsely accepted ${pct(junkAccepted, junkN)}, distance p1 ${quant(junk, 0.01)} p10 ${quant(junk, 0.1)} p50 ${quant(junk, 0.5)}`);
const top = Object.entries(confusion).sort((a, b) => b[1] - a[1]).slice(0, 8);
if (top.length) console.log("top confusions:", top.map(([k, v]) => `${k} x${v}`).join(", "));

console.log("\naccept  margin | genuine accepted+correct | wrong accepted | scribbles accepted");
for (const acc of [0.5, 0.55, 0.6, 0.65]) {
  for (const margin of [0, 0.05, 0.1, 0.15]) {
    const pass = (x) => x.s >= acc && x.s - x.r >= margin;
    const good = samples.filter((x) => x.ok && pass(x)).length;
    const bad = samples.filter((x) => !x.ok && pass(x)).length;
    const fa = junkSamples.filter(pass).length;
    console.log(`  ${acc.toFixed(2)}   ${margin.toFixed(2)}  | ${pct(good, samples.length).padStart(24)} | ${pct(bad, samples.length).padStart(14)} | ${pct(fa, junkSamples.length).padStart(18)}`);
  }
}

if (assertMode) {
  const failures = [];
  if (correct / total < 0.97) failures.push(`top-1 accuracy ${pct(correct, total)} < 97%`);
  if (accepted / total < 0.94) failures.push(`accepted+correct ${pct(accepted, total)} < 94%`);
  if (wrongAccepted / total > 0.005) failures.push(`accepted+wrong ${pct(wrongAccepted, total)} > 0.5%`);
  if (junkAccepted / junkN > 0.1) failures.push(`scribbles accepted ${pct(junkAccepted, junkN)} > 10%`);
  if (failures.length) {
    console.error(`\nRECOGNIZER REGRESSION:\n  ${failures.join("\n  ")}`);
    process.exit(1);
  }
  console.log("\nrecognizer accuracy gate: OK");
}
