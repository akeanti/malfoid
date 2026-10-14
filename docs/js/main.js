import { initAnatomy } from "./anatomy.js";
import { initConsole } from "./console.js";
import { initDemo } from "./demo.js";
import { initFlasher } from "./flasher.js";
import { initHero } from "./hero.js";
import { initTabs } from "./tabs.js";

const reduceMotion = matchMedia("(prefers-reduced-motion: reduce)").matches;

// Each piece is independent: one failing (old browser, missing API) must not take the rest down.
const safely = (name, fn) => {
  try {
    fn();
  } catch (err) {
    console.error(`[malfoid] ${name} failed`, err);
  }
};

safely("tabs", () => initTabs());
safely("hero", () => initHero(document.getElementById("hero-art"), { reduceMotion }));
safely("spellbook", () => initDemo({ reduceMotion }));
safely("anatomy", () => initAnatomy());
safely("console", () => initConsole());
safely("flasher", () => initFlasher());
safely("header", initHeader);
safely("reveal", initReveal);

function initHeader() {
  const header = document.getElementById("site-header");
  const onScroll = () => header.classList.toggle("scrolled", scrollY > 24);
  addEventListener("scroll", onScroll, { passive: true });
  onScroll();

  // Mobile menu.
  const btn = document.getElementById("menu-btn");
  const menu = document.getElementById("mobile-menu");
  const setOpen = (open) => {
    menu.hidden = !open;
    btn.setAttribute("aria-expanded", String(open));
    btn.setAttribute("aria-label", open ? "Close menu" : "Open menu");
  };
  btn.addEventListener("click", () => setOpen(menu.hidden));
  menu.addEventListener("click", (e) => { if (e.target.closest("a")) setOpen(false); });
  addEventListener("keydown", (e) => { if (e.key === "Escape") setOpen(false); });

  // Scrollspy: highlight the nav link for the section in view.
  const links = new Map([...document.querySelectorAll("[data-spy]")].map((a) => [a.getAttribute("href").slice(1), a]));
  const io = new IntersectionObserver((entries) => {
    for (const e of entries) {
      if (!e.isIntersecting) continue;
      links.forEach((a, id) => a.classList.toggle("active", id === e.target.id));
    }
  }, { rootMargin: "-45% 0px -50% 0px" });
  links.forEach((_, id) => { const el = document.getElementById(id); if (el) io.observe(el); });
}

function initReveal() {
  const els = document.querySelectorAll(".reveal");
  if (reduceMotion || !("IntersectionObserver" in window)) {
    els.forEach((el) => el.classList.add("in"));
    return;
  }
  const io = new IntersectionObserver((entries) => {
    for (const e of entries) {
      if (!e.isIntersecting) continue;
      e.target.classList.add("in");
      io.unobserve(e.target);
    }
  }, { rootMargin: "0px 0px -8% 0px", threshold: 0.06 });
  els.forEach((el) => {
    const siblings = [...el.parentElement.children].filter((c) => c.classList.contains("reveal"));
    el.style.transitionDelay = `${Math.min(siblings.indexOf(el), 4) * 70}ms`;
    io.observe(el);
  });
}
