// Hero: the cameo floats a little in front of the wand (pointer parallax), and
// the wand tip sheds sparkles, with a brighter flourish every few seconds.
import { COLORS, Sparkles, fitCanvas } from "./canvas.js";

export function initHero(root, { reduceMotion }) {
  if (!root) return;
  const canvas = root.querySelector("canvas");
  const tipEl = root.querySelector("#wand-tip");
  const layers = [...root.querySelectorAll(".layer")];
  const sparkles = new Sparkles(420);
  const view = fitCanvas(canvas);

  // Parallax: layers drift opposite ways with the pointer, giving depth.
  if (!reduceMotion && matchMedia("(pointer: fine)").matches) {
    window.addEventListener("pointermove", (e) => {
      const nx = e.clientX / innerWidth - 0.5;
      const ny = e.clientY / innerHeight - 0.5;
      for (const layer of layers) {
        const depth = Number(layer.dataset.depth || 0);
        layer.style.transform = `translate3d(${nx * depth}px, ${ny * depth}px, 0)`;
      }
    }, { passive: true });
  }
  if (reduceMotion) return;

  const tip = () => {
    const a = tipEl.getBoundingClientRect(), b = canvas.getBoundingClientRect();
    return [a.left + a.width / 2 - b.left, a.top + a.height / 2 - b.top];
  };

  let visible = true;
  new IntersectionObserver(([e]) => { visible = e.isIntersecting; }).observe(root);
  let last = performance.now(), nextFlourish = last + 1800;
  function frame(now) {
    const dt = Math.min(0.05, (now - last) / 1000);
    last = now;
    if (visible) {
      const [x, y] = tip();
      if (Math.random() < 0.6) sparkles.emit(x, y, { count: 1, speed: 38, life: [0.7, 1.6], size: [0.5, 1.8] });
      if (now > nextFlourish) {
        sparkles.emit(x, y, { count: 34, speed: 120, life: [0.6, 1.5], colors: [COLORS.silverHi, COLORS.emerald, COLORS.platinum] });
        nextFlourish = now + 4200 + Math.random() * 2400;
      }
      sparkles.step(dt);
      view.ctx.clearRect(0, 0, view.w, view.h);
      sparkles.draw(view.ctx);
    }
    requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);
}
