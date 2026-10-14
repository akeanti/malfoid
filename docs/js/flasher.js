// Browser flashing via ESP Web Tools. Firmware images are published next to
// this page by .github/workflows/pages.yml.

export function initFlasher() {
  const installer = document.getElementById("installer");
  const note = document.getElementById("fw-note");
  if (!installer || !note) return;

  const check = async (board) => {
    const manifestUrl = `firmware/manifest-${board}.json`;
    installer.setAttribute("manifest", manifestUrl);
    try {
      const manifest = await (await fetch(manifestUrl, { cache: "no-store" })).json();
      const part = manifest.builds[0].parts[0].path;
      const bin = await fetch(new URL(part, new URL(manifestUrl, location.href)), { method: "HEAD", cache: "no-store" });
      if (!bin.ok) throw new Error("missing image");
      note.textContent = `Ready: ${manifest.name}, firmware ${manifest.version}.`;
    } catch {
      note.textContent = "No firmware published here yet. Push to main and the GitHub Actions workflow builds and publishes it.";
    }
  };

  for (const radio of document.querySelectorAll('input[name="board"]')) {
    radio.addEventListener("change", () => radio.checked && check(radio.value));
  }
  check(document.querySelector('input[name="board"]:checked')?.value ?? "xiao");
}
