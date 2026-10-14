import { initConsole } from "./console.js";
import { initDemo } from "./demo.js";
import { initFlasher } from "./flasher.js";
import { initHero } from "./hero.js";
import { initMotes } from "./motes.js";

const reduceMotion = matchMedia("(prefers-reduced-motion: reduce)").matches;

// Each piece is independent: one failing (old browser, missing API) must not take the rest down.
const safely = (name, fn) => {
  try {
    fn();
  } catch (err) {
    console.error(`[malfoid] ${name} failed`, err);
  }
};

safely("motes", () => initMotes(document.getElementById("motes"), { reduceMotion }));
safely("hero", () => initHero(document.getElementById("hero-canvas"), { reduceMotion }));
safely("spellbook", () => initDemo({ reduceMotion }));
safely("console", () => initConsole());
safely("flasher", () => initFlasher());

// Gentle reveal-on-scroll.
const revealables = document.querySelectorAll(".reveal");
if (reduceMotion || !("IntersectionObserver" in window)) {
  revealables.forEach((el) => el.classList.add("in"));
} else {
  const io = new IntersectionObserver(
    (entries) => {
      for (const e of entries) {
        if (!e.isIntersecting) continue;
        e.target.classList.add("in");
        io.unobserve(e.target);
      }
    },
    { rootMargin: "0px 0px -8% 0px", threshold: 0.08 },
  );
  revealables.forEach((el, i) => {
    el.style.transitionDelay = `${(i % 4) * 60}ms`;
    io.observe(el);
  });
}
