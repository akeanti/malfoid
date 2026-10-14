// Slow silver motes drifting behind everything.
import { fitCanvas } from "./canvas.js";

export function initMotes(canvas, { reduceMotion }) {
  let motes = [];
  const view = fitCanvas(canvas, (w, h) => {
    const n = Math.round(Math.min(70, (w * h) / 22000));
    motes = Array.from({ length: n }, () => ({
      x: Math.random() * w,
      y: Math.random() * h,
      r: 0.4 + Math.random() * 1.3,
      vy: -(2 + Math.random() * 6),
      vx: (Math.random() - 0.5) * 3,
      phase: Math.random() * Math.PI * 2,
      emerald: Math.random() < 0.18,
    }));
    if (reduceMotion) draw(0);
  });

  function draw(t) {
    const { ctx, w, h } = view;
    ctx.clearRect(0, 0, w, h);
    for (const m of motes) {
      const tw = 0.35 + 0.65 * (0.5 + 0.5 * Math.sin(t * 0.0012 + m.phase));
      ctx.globalAlpha = 0.12 + 0.3 * tw;
      ctx.fillStyle = m.emerald ? "#2fd38a" : "#dfe5ea";
      ctx.beginPath();
      ctx.arc(m.x, m.y, m.r, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.globalAlpha = 1;
  }

  if (reduceMotion) return;
  let last = performance.now();
  function frame(now) {
    const dt = Math.min(0.05, (now - last) / 1000);
    last = now;
    for (const m of motes) {
      m.y += m.vy * dt;
      m.x += m.vx * dt;
      if (m.y < -4) { m.y = view.h + 4; m.x = Math.random() * view.w; }
      if (m.x < -4) m.x = view.w + 4;
      if (m.x > view.w + 4) m.x = -4;
    }
    draw(now);
    requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);
}
