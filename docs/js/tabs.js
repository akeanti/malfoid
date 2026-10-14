// Accessible tabs for any [data-tabs] container holding a [role=tablist].
// Arrow keys move between tabs, Home/End jump; panels are toggled via `hidden`.

export function initTabs(scope = document) {
  for (const root of scope.querySelectorAll("[data-tabs]")) {
    const tabs = [...root.querySelectorAll('[role="tab"]')];
    const select = (tab, focus = false) => {
      for (const t of tabs) {
        const on = t === tab;
        t.setAttribute("aria-selected", String(on));
        t.tabIndex = on ? 0 : -1;
        document.getElementById(t.getAttribute("aria-controls")).hidden = !on;
      }
      if (focus) tab.focus();
      root.dispatchEvent(new CustomEvent("tabchange", { detail: { id: tab.id } }));
    };
    tabs.forEach((tab, i) => {
      tab.type = "button";
      tab.addEventListener("click", () => select(tab));
      tab.addEventListener("keydown", (e) => {
        const next = { ArrowRight: i + 1, ArrowLeft: i - 1, Home: 0, End: tabs.length - 1 }[e.key];
        if (next === undefined) return;
        e.preventDefault();
        select(tabs[(next + tabs.length) % tabs.length], true);
      });
    });
  }
}
