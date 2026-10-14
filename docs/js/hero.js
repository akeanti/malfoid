// Hero stage: the wand draws her signature "M" in light, on a loop.
import { COLORS, Sparkles, easeInOut, fitCanvas, glowStroke, walk } from "./canvas.js";
import { SPELLBOOK } from "./spellbook.js";

const DRAW = 1.9, HOLD = 1.5, FADE = 0.7, REST = 1.1;
const CYCLE = DRAW + HOLD + FADE + REST;

export function initHero(canvas, { reduceMotion }) {
  const glyph = SPELLBOOK.templates.find((t) => t.spell === "malfoid").path;
  let pts = [];
  const sparkles = new Sparkles(500);

  const view = fitCanvas(canvas, (w, h) => {
    const size = Math.min(w, h) * 0.5;
    const ox = w * 0.44 - size / 2;
    const oy = h * 0.43 + size / 2;
    // Densify so the head moves smoothly and the corners stay crisp.
    pts = [];
    for (let i = 1; i < glyph.length; i++) {
      const [x0, y0] = glyph[i - 1], [x1, y1] = glyph[i];
      for (let k = 0; k < 24; k++) {
        const t = k / 24;
        pts.push([ox + (x0 + (x1 - x0) * t) * size, oy - (y0 + (y1 - y0) * t) * size]);
      }
    }
    pts.push([ox + glyph[glyph.length - 1][0] * size, oy - glyph[glyph.length - 1][1] * size]);
    if (reduceMotion) render(DRAW + 0.5, 0);
  });

  function drawWand(ctx, tip, w, h) {
    const wrist = [w * 1.02, h * 1.18];
    const dx = wrist[0] - tip[0], dy = wrist[1] - tip[1];
    const len = Math.hypot(dx, dy) || 1;
    const ux = dx / len, uy = dy / len;   // tip -> handle
    const nx = -uy, ny = ux;               // normal
    const L = h * 0.78;
    const at = (s) => [tip[0] + ux * L * s, tip[1] + uy * L * s];
    const width = (s) => 1.4 + s * 6.5;

    ctx.save();
    // shaft + handle silhouette
    ctx.beginPath();
    for (let i = 0; i <= 20; i++) {
      const s = i / 20, [x, y] = at(s), r = width(s);
      ctx.lineTo(x + nx * r, y + ny * r);
    }
    for (let i = 20; i >= 0; i--) {
      const s = i / 20, [x, y] = at(s), r = width(s);
      ctx.lineTo(x - nx * r, y - ny * r);
    }
    ctx.closePath();
    const g = ctx.createLinearGradient(tip[0], tip[1], ...at(1));
    g.addColorStop(0, "#2a322e");
    g.addColorStop(0.55, "#161b18");
    g.addColorStop(1, "#0d100e");
    ctx.fillStyle = g;
    ctx.fill();
    ctx.strokeStyle = "rgba(201,209,217,0.35)";
    ctx.lineWidth = 0.8;
    ctx.stroke();

    // silver bands on the handle
    for (const s of [0.6, 0.64, 0.93]) {
      const [x, y] = at(s), r = width(s) + 0.6;
      ctx.beginPath();
      ctx.moveTo(x + nx * r, y + ny * r);
      ctx.lineTo(x - nx * r, y - ny * r);
      ctx.strokeStyle = COLORS.silver;
      ctx.lineWidth = 2.2;
      ctx.stroke();
    }
    // the emerald, where her thumb rests
    const [gx, gy] = at(0.74);
    ctx.beginPath();
    ctx.ellipse(gx - nx * 1.5, gy - ny * 1.5, 3.2, 5, Math.atan2(uy, ux), 0, Math.PI * 2);
    ctx.fillStyle = COLORS.emerald;
    ctx.shadowColor = COLORS.emerald;
    ctx.shadowBlur = 10;
    ctx.fill();
    ctx.restore();

    // tip glow
    const rg = ctx.createRadialGradient(tip[0], tip[1], 0, tip[0], tip[1], 26);
    rg.addColorStop(0, "rgba(255,255,255,0.95)");
    rg.addColorStop(0.25, "rgba(201,247,226,0.55)");
    rg.addColorStop(1, "rgba(47,211,138,0)");
    ctx.fillStyle = rg;
    ctx.beginPath();
    ctx.arc(tip[0], tip[1], 26, 0, Math.PI * 2);
    ctx.fill();
  }

  let prevPhase = 0;
  function render(t, dt) {
    const { ctx, w, h } = view;
    if (!pts.length) return;
    ctx.clearRect(0, 0, w, h);
    const phase = t % CYCLE;

    let progress = 1, alpha = 1, tip;
    if (phase < DRAW) {
      progress = easeInOut(phase / DRAW);
    } else if (phase < DRAW + HOLD) {
      progress = 1;
    } else if (phase < DRAW + HOLD + FADE) {
      alpha = 1 - (phase - DRAW - HOLD) / FADE;
    } else {
      alpha = 0;
    }
    const { point, path } = walk(pts, progress);

    if (phase < DRAW) {
      tip = point;
      sparkles.emit(point[0], point[1], { count: 3, speed: 55 });
    } else if (phase < DRAW + HOLD) {
      if (prevPhase < DRAW) {  // the flourish
        for (const p of pts.filter((_, i) => i % 6 === 0)) sparkles.emit(p[0], p[1], { count: 2, speed: 35, life: [0.6, 1.4] });
      }
      const k = (phase - DRAW) / HOLD;
      tip = [point[0] + Math.sin(k * Math.PI) * 10, point[1] - Math.sin(k * Math.PI) * 18];
    } else {
      // drift back to the start for the next cast
      const k = easeInOut(Math.min(1, (phase - DRAW - HOLD) / (FADE + REST)));
      const end = pts[pts.length - 1], start = pts[0];
      tip = [end[0] + (start[0] - end[0]) * k, end[1] + 10 * Math.sin(k * Math.PI) + (start[1] - end[1]) * k - 18 * (1 - k)];
    }
    prevPhase = phase;

    if (alpha > 0) {
      const holding = phase >= DRAW && phase < DRAW + HOLD;
      const pulse = holding ? 0.5 + 0.5 * Math.sin((phase - DRAW) * 5) : 0;
      glowStroke(ctx, path, {
        color: COLORS.silverHi,
        glow: holding ? COLORS.emerald : COLORS.silver,
        width: 3.2,
        blur: 18 + pulse * 14,
        alpha,
      });
    }
    sparkles.step(dt);
    sparkles.draw(ctx);
    drawWand(ctx, tip, w, h);
  }

  if (reduceMotion) return;
  let visible = true;
  new IntersectionObserver(([e]) => { visible = e.isIntersecting; }).observe(canvas);
  let last = performance.now(), clock = 0;
  function frame(now) {
    const real = (now - last) / 1000;
    last = now;
    if (visible) {
      clock += Math.min(real, 0.5);        // timeline follows the wall clock, even at low frame rates
      render(clock, Math.min(real, 0.05)); // physics stays stable
    }
    requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);
}
