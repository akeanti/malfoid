// Anatomy figure: X-ray toggle, and hovering a legend entry or a marker
// highlights its partner.

export function initAnatomy() {
  const panel = document.getElementById("anatomy-panel");
  if (!panel) return;
  const toggle = document.getElementById("xray-toggle");
  toggle.addEventListener("change", () => panel.classList.toggle("xray-on", toggle.checked));

  const items = [...panel.querySelectorAll("[data-part]")];
  const hot = (part) => items.forEach((el) => el.classList.toggle("hot", el.dataset.part === part));
  for (const el of items) {
    el.addEventListener("pointerenter", () => hot(el.dataset.part));
    el.addEventListener("pointerleave", () => hot(null));
  }
  // Looking inside makes more sense once the parts are labelled: open X-ray
  // automatically the first time the figure scrolls into view, then close it.
  if (!matchMedia("(prefers-reduced-motion: reduce)").matches) {
    const io = new IntersectionObserver(([e]) => {
      if (!e.isIntersecting) return;
      io.disconnect();
      setTimeout(() => { if (!toggle.checked) { panel.classList.add("xray-on"); setTimeout(() => panel.classList.toggle("xray-on", toggle.checked), 2400); } }, 700);
    }, { threshold: 0.6 });
    io.observe(panel.querySelector(".wand-figure"));
  }
}
