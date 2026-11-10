import { initializeRevealMotion } from "./runtime/reveal";
import { demoVolumeProfiles } from "./runtime/clean-room/profiles";

const releaseStartupGate = (fallback = false): void => {
  const root = document.documentElement;
  root.classList.remove("press-startup-pending");
  if (fallback) {
    root.classList.add("press-startup-fallback");
    const sections = Array.from(document.querySelectorAll<HTMLElement>(".press-volume-section"));
    const path = `${location.pathname.replace(/\/+$/, "")}/`;
    const active = sections.find((section) => section.dataset.pressVolume === path);
    if (active) {
      document.title = document.body.dataset.pressCatalogueTitle ?? "Press";
      root.classList.add("press-volumes-open", "press-in-volumes");
      sections.forEach((section) => { section.hidden = section !== active; });
      active.scrollIntoView();
      document.querySelector(".press-back")?.addEventListener("click", () => {
        location.assign(document.body.dataset.pressCatalogue ?? "/press/");
      });
    }
    document.querySelectorAll<HTMLButtonElement>(".press-rail-item").forEach((button, index) => {
      button.addEventListener("click", () => {
        const route = sections[index]?.dataset.pressVolume;
        if (route) location.assign(route);
      });
    });
  }
};

const start = async (): Promise<void> => {
  const about = document.querySelector<HTMLDialogElement>(".press-about-dialog");
  document.querySelectorAll("[data-press-credits-open]").forEach((link) => link.addEventListener("click", (event) => {
    event.preventDefault();
    if (about && !about.open) about.showModal();
  }));
  document.querySelector("[data-press-about-open]")?.addEventListener("click", () => {
    document.dispatchEvent(new Event("press:about"));
  });
  document.querySelector("[data-press-about-top]")?.addEventListener("click", (event) => {
    event.preventDefault();
    window.scrollTo({ top: 0, behavior: "instant" });
    document.querySelector<HTMLElement>(".home-brand")?.focus({ preventScroll: true });
  });
  document.querySelector("[data-press-about-close]")?.addEventListener("click", () => about?.close());
  about?.addEventListener("click", (event) => {
    if (event.target !== about) return;
    const bounds = about.getBoundingClientRect();
    if (event.clientX < bounds.left || event.clientX > bounds.right
      || event.clientY < bounds.top || event.clientY > bounds.bottom) about.close();
  });
  try {
    initializeRevealMotion();
    const { mountCleanRoomCatalogue } = await import("./runtime/clean-room");
    if (!(await mountCleanRoomCatalogue({ profiles: demoVolumeProfiles }))) {
      releaseStartupGate(true);
    }
  } catch (error) {
    releaseStartupGate(true);
    console.warn("Press scene failed to initialise; showing the DOM fallback.", error);
  }
};

if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", () => void start(), { once: true });
} else {
  void start();
}
