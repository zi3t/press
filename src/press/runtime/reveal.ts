export const initializeRevealMotion = (): void => {
  const reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)");
  const revealNodes = Array.from(document.querySelectorAll("[data-reveal]"));

  const revealHashTarget = () => {
    if (!window.location.hash) return;

    const id = decodeURIComponent(window.location.hash.slice(1));
    const target = document.getElementById(id);
    if (!target) return;

    target.classList.add("is-visible");
    target.querySelectorAll("[data-reveal]").forEach((node) => {
      node.classList.add("is-visible");
    });

    requestAnimationFrame(() => target.scrollIntoView({ block: "start" }));
  };

  if (reduceMotion.matches || !("IntersectionObserver" in window)) {
    revealNodes.forEach((node) => node.classList.add("is-visible"));
    revealHashTarget();
    return;
  }

  document.documentElement.classList.add("home-motion");

  const observer = new IntersectionObserver(
    (entries) => {
      entries.forEach((entry) => {
        if (!entry.isIntersecting) return;

        entry.target.classList.add("is-visible");
        observer.unobserve(entry.target);
      });
    },
    {
      threshold: 0.12,
      rootMargin: "0px 0px -8% 0px",
    },
  );

  revealHashTarget();
  revealNodes.forEach((node) => observer.observe(node));
  window.addEventListener("hashchange", revealHashTarget);
  window.addEventListener("load", revealHashTarget, { once: true });
};
