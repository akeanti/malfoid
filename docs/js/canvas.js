// Small canvas helpers shared by the hero, the spellbook demo and the console.

export const COLORS = {
  silver: "#c9d1d9",
  silverHi: "#f4f7f9",
  emerald: "#2fd38a",
  platinum: "#f3ead7",
  danger: "#ff8a7a",
};

// Keeps a canvas's backing store matched to its CSS size and device pixel ratio.
// Calls onResize(width, height) in CSS pixels whenever it changes. The first
// call is deferred a microtask so callers can use the returned state inside it.
export function fitCanvas(canvas, onResize) {
  const ctx = canvas.getContext("2d");
  const state = { ctx, w: 0, h: 0, dpr: 1 };
  let ready = false;
  const apply = () => {
    const rect = canvas.getBoundingClientRect();
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    const w = Math.max(1, Math.round(rect.width));
    const h = Math.max(1, Math.round(rect.height));
    if (w === state.w && h === state.h && dpr === state.dpr) return;
    Object.assign(state, { w, h, dpr });
    canvas.width = Math.round(w * dpr);
    canvas.height = Math.round(h * dpr);
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    if (ready) onResize?.(w, h);
  };
  new ResizeObserver(apply).observe(canvas);
  apply();
  queueMicrotask(() => {
    ready = true;
    onResize?.(state.w, state.h);
  });
  return state;
}

export const easeInOut = (t) => (t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2);

export function hexToRgb(hex) {
  const n = parseInt(hex.replace("#", ""), 16);
  return [(n >> 16) & 255, (n >> 8) & 255, n & 255];
}

// Additive sparkles: little four-point stars that drift, fall and fade.
export class Sparkles {
  constructor(max = 400) {
    this.max = max;
    this.items = [];
  }

  emit(x, y, { count = 3, spread = 1.4, speed = 60, colors = [COLORS.silverHi, COLORS.emerald, COLORS.platinum], life = [0.5, 1.1], size = [0.6, 2.2] } = {}) {
    for (let i = 0; i < count; i++) {
      if (this.items.length >= this.max) this.items.shift();
      const a = Math.random() * Math.PI * 2;
      const v = speed * (0.2 + Math.random() * spread);
      this.items.push({
        x, y,
        vx: Math.cos(a) * v, vy: Math.sin(a) * v - speed * 0.3,
        life: life[0] + Math.random() * (life[1] - life[0]),
        age: 0,
        size: size[0] + Math.random() * (size[1] - size[0]),
        color: colors[(Math.random() * colors.length) | 0],
        spin: Math.random() * Math.PI,
      });
    }
  }

  step(dt) {
    for (const p of this.items) {
      p.age += dt;
      p.x += p.vx * dt;
      p.y += p.vy * dt;
      p.vx *= 0.96;
      p.vy = p.vy * 0.96 + 40 * dt; // a little gravity
      p.spin += dt * 3;
    }
    this.items = this.items.filter((p) => p.age < p.life);
  }

  draw(ctx) {
    ctx.save();
    ctx.globalCompositeOperation = "lighter";
    for (const p of this.items) {
      const k = 1 - p.age / p.life;
      const s = p.size * (0.6 + 0.8 * k);
      ctx.globalAlpha = Math.min(1, k * 1.3);
      ctx.fillStyle = p.color;
      ctx.beginPath();
      for (let i = 0; i < 8; i++) {
        const r = i % 2 === 0 ? s * 2.2 : s * 0.55;
        const a = p.spin + (i * Math.PI) / 4;
        ctx.lineTo(p.x + Math.cos(a) * r, p.y + Math.sin(a) * r);
      }
      ctx.fill();
    }
    ctx.restore();
  }

  get alive() {
    return this.items.length > 0;
  }
}

// Strokes a polyline with a soft glow underneath a crisp core.
export function glowStroke(ctx, pts, { color = COLORS.silverHi, glow = COLORS.silver, width = 2.4, blur = 16, alpha = 1, dash = null } = {}) {
  if (pts.length < 2) return;
  ctx.save();
  ctx.globalAlpha = alpha;
  ctx.lineCap = "round";
  ctx.lineJoin = "round";
  if (dash) ctx.setLineDash(dash);
  ctx.beginPath();
  ctx.moveTo(pts[0][0], pts[0][1]);
  for (let i = 1; i < pts.length; i++) ctx.lineTo(pts[i][0], pts[i][1]);
  ctx.shadowColor = glow;
  ctx.shadowBlur = blur;
  ctx.strokeStyle = glow;
  ctx.globalAlpha = alpha * 0.45;
  ctx.lineWidth = width * 3;
  ctx.stroke();
  ctx.shadowBlur = blur * 0.5;
  ctx.globalAlpha = alpha;
  ctx.strokeStyle = color;
  ctx.lineWidth = width;
  ctx.stroke();
  ctx.restore();
}

// Returns a point at fraction t (0..1) of a polyline's length, plus the sub-path up to it.
export function walk(pts, t) {
  let total = 0;
  const seg = [];
  for (let i = 1; i < pts.length; i++) {
    const d = Math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]);
    seg.push(d);
    total += d;
  }
  let target = Math.max(0, Math.min(1, t)) * total;
  const path = [pts[0]];
  for (let i = 1; i < pts.length; i++) {
    if (target <= seg[i - 1]) {
      const k = seg[i - 1] ? target / seg[i - 1] : 0;
      const p = [pts[i - 1][0] + (pts[i][0] - pts[i - 1][0]) * k, pts[i - 1][1] + (pts[i][1] - pts[i - 1][1]) * k];
      path.push(p);
      return { point: p, path };
    }
    target -= seg[i - 1];
    path.push(pts[i]);
  }
  return { point: pts[pts.length - 1], path };
}
