export type CleanRoomPressMode = "catalogue" | "volumes";

export type CleanRoomRouteSource =
  | "pick"
  | "rail"
  | "keyboard"
  | "back-control"
  | "popstate"
  | "deep-link"
  | "scroll";

export interface CleanRoomRouteSnapshot {
  readonly mode: CleanRoomPressMode;
  readonly productKind: "book" | "film" | "signal";
  readonly currentIndex: number;
  readonly currentAddress: string;
  readonly pendingDeepLinkIndex: number;
  readonly catalogueScrollY: number;
}

interface CleanRoomRouteCallbacks {
  readonly items: readonly HTMLElement[];
  readonly links: readonly HTMLAnchorElement[];
  readonly catalogueScrollTop: (index: number) => number;
  readonly catalogueReady?: () => boolean;
  readonly onBeforeVolume: (index: number, source: CleanRoomRouteSource) => void;
  readonly onBeforeCatalogue: (index: number, source: CleanRoomRouteSource) => void;
  readonly onModeChange: (
    mode: CleanRoomPressMode,
    previous: CleanRoomPressMode,
    source: CleanRoomRouteSource
  ) => void;
  readonly onIndexChange: (index: number, source: CleanRoomRouteSource) => void;
  readonly onWake: (duration?: number) => void;
}

export interface CleanRoomRouteController {
  readonly sections: readonly HTMLElement[];
  readonly figures: readonly (HTMLElement | null)[];
  readonly available: boolean;
  snapshot: () => CleanRoomRouteSnapshot;
  activate: (index: number, event?: MouseEvent) => boolean;
  returnToCatalogue: (source: CleanRoomRouteSource) => boolean;
  setCatalogueIndex: (index: number) => void;
  refreshActiveInk: () => void;
  settlePendingDeepLink: () => boolean;
}

interface ModifiedInputEvent {
  readonly button?: number;
  readonly metaKey: boolean;
  readonly ctrlKey: boolean;
  readonly shiftKey: boolean;
  readonly altKey: boolean;
}

const hasModifiedClick = (event: ModifiedInputEvent): boolean => (
  (event.button ?? 0) > 0
  || event.metaKey
  || event.ctrlKey
  || event.shiftKey
  || event.altKey
);

const instantScroll = (top: number): void => {
  window.scrollTo({ top, behavior: "instant" });
};

export const installCleanRoomRouting = (
  callbacks: CleanRoomRouteCallbacks
): CleanRoomRouteController => {
  const root = document.documentElement;
  const main = document.querySelector<HTMLElement>(".home-page main");
  const hero = document.querySelector<HTMLElement>(".home-hero");
  const volumes = document.querySelector<HTMLElement>(".press-volumes");
  const sections = Array.from(
    document.querySelectorAll<HTMLElement>(".press-volume-section")
  );
  const figures = sections.map((section) => (
    section.querySelector<HTMLElement>(".press-volume-figure")
  ));
  const filmLink = document.querySelector<HTMLAnchorElement>(".press-film-link");
  const signalLink = document.querySelector<HTMLAnchorElement>(".press-signal-link");
  const productLinks = [...callbacks.links, ...(filmLink ? [filmLink] : []), ...(signalLink ? [signalLink] : [])];
  const products = productLinks.map((link, index) => ({
    link,
    kind: index < callbacks.links.length ? "book" as const
      : link === signalLink ? "signal" as const : "film" as const
  }));
  const rail = document.querySelector<HTMLElement>(".press-rail");
  const railButtons = Array.from(
    document.querySelectorAll<HTMLButtonElement>(".press-rail-item")
  );
  const railFills = railButtons.map((button) => (
    button.querySelector<HTMLElement>(".press-rail-fill")
  ));
  const backButton = document.querySelector<HTMLButtonElement>(".press-back");
  const configuredCatalogue = document.body.dataset.pressCatalogue ?? "/press/";
  const cataloguePath = configuredCatalogue === "/"
    ? "/"
    : `${configuredCatalogue.replace(/\/+$/, "")}/`;
  const routePaths = productLinks.map((link) => new URL(
    link.dataset.pressRoute ?? link.href,
    window.location.href
  ).pathname);

  const cleanAddress = (pathname: string): string => {
    const url = new URL(pathname, window.location.href);
    url.search = "";
    return `${url.pathname}${url.search}${url.hash}`;
  };
  const volumeAddress = (index: number): string => cleanAddress(routePaths[index] ?? cataloguePath);
  const catalogueAddress = cleanAddress(cataloguePath);
  const pathIndex = (): number => routePaths.indexOf(window.location.pathname);
  const deepLinkIndex = pathIndex();
  let mode: CleanRoomPressMode = deepLinkIndex >= 0 ? "volumes" : "catalogue";
  let currentIndex = Math.max(0, deepLinkIndex);
  let currentAddress = cleanAddress(`${window.location.pathname}${window.location.hash}`);
  let pendingDeepLinkIndex = deepLinkIndex;
  let catalogueScrollY = window.scrollY;
  let catalogueRestoreFrame = 0;
  let activeInks: string[] = [];

  const updateDocumentTitle = (): void => {
    const siteName = document.body.dataset.pressSiteName ?? "Press";
    const catalogueTitle = document.body.dataset.pressCatalogueTitle ?? siteName;
    const productTitle = sections[currentIndex]
      ?.querySelector(".press-volume-title")?.textContent?.trim();
    const socialTitle = mode === "volumes" && productTitle
      ? `${productTitle} — ${siteName}`
      : catalogueTitle;
    document.title = catalogueTitle;
    document.querySelectorAll<HTMLMetaElement>('meta[property="og:title"], meta[name="twitter:title"]')
      .forEach((meta) => { meta.content = socialTitle; });
  };

  const available = Boolean(
    main
    && volumes
    && sections.length === products.length
    && figures.every(Boolean)
  );

  if (available && main && rail && backButton && !rail.closest(".press-route-controls")) {
    const controls = document.createElement("div");
    controls.className = "press-route-controls";
    main.append(controls);
    controls.append(backButton, rail);
  }

  const updateDocumentHeight = (): void => {
    if (!available || !main || !volumes) return;
    if (mode === "volumes") {
      main.style.height = `${Math.ceil(volumes.offsetHeight)}px`;
    } else {
      main.style.removeProperty("height");
    }
  };

  const updateAccess = (): void => {
    callbacks.items.forEach((item) => {
      item.inert = mode === "volumes";
    });
    if (filmLink) filmLink.inert = mode === "volumes";
    if (signalLink) signalLink.inert = mode === "volumes";
  };

  const focusDestination = (index: number): void => {
    const target = mode === "volumes"
      ? sections[index]?.querySelector<HTMLElement>(".press-volume-title")
      : productLinks[index];
    if (!target) return;
    if (mode === "volumes") target.tabIndex = -1;
    target.focus({ preventScroll: true });
  };

  const closeRailPreview = (): void => {
    document.querySelector(".press-catalog")?.classList.remove("is-index-preview");
    document.body.classList.remove("press-index-preview");
    railButtons.forEach((button) => button.classList.remove("is-preview"));
    railFills.forEach((fill) => fill?.style.removeProperty("--rail-scale"));
  };

  const previewRailIndex = (index: number): void => {
    // The indicator wave and title are route navigation, so they remain live
    // both on the shelf and inside a volume. Only the shelf preview owns the
    // catalogue scrim: applying it in `volumes` would dim the open book page.
    if (mode === "catalogue") {
      document.querySelector(".press-catalog")?.classList.add("is-index-preview");
      document.body.classList.add("press-index-preview");
    }
    railButtons.forEach((button, buttonIndex) => {
      button.classList.toggle("is-preview", buttonIndex === index);
    });
    railFills.forEach((fill, fillIndex) => {
      if (!fill) return;
      const distance = Math.abs(index - fillIndex);
      const scale = Math.max(
        1,
        Math.cos(distance / Math.max(1, railFills.length) * Math.PI) * 2 + 2.5
      );
      fill.style.setProperty("--rail-scale", scale.toFixed(3));
    });
  };

  const syncActiveInk = (index: number): void => {
    const section = sections[index];
    if (section) {
      const ink = activeInks[index] || getComputedStyle(section)
        .getPropertyValue("--press-volume-ink")
        .trim();
      activeInks[index] = ink;
      root.style.setProperty(
        "--press-active-ink",
        ink
      );
    }
  };

  const refreshActiveInks = (): void => {
    activeInks = sections.map((section) => getComputedStyle(section)
      .getPropertyValue("--press-volume-ink")
      .trim());
    syncActiveInk(currentIndex);
  };

  const setCurrentIndex = (next: number, source: CleanRoomRouteSource): void => {
    const bounded = Math.min(products.length - 1, Math.max(0, next));
    currentIndex = bounded;
    if (mode === "volumes") updateDocumentTitle();
    // Catalogue scroll only changes the rail marker. Reading a volume
    // section's computed ink here forced a full style flush at every book
    // boundary in Safari. Volume ink is premeasured outside the scroll path.
    if (mode === "volumes" || source !== "scroll") syncActiveInk(bounded);
    railButtons.forEach((button, index) => {
      const current = index === bounded;
      button.classList.toggle("is-current", current);
      if (current) button.setAttribute("aria-current", "true");
      else button.removeAttribute("aria-current");
    });
    callbacks.onIndexChange(bounded, source);
  };

  const setMode = (next: CleanRoomPressMode, source: CleanRoomRouteSource): void => {
    if (catalogueRestoreFrame) {
      cancelAnimationFrame(catalogueRestoreFrame);
      catalogueRestoreFrame = 0;
    }
    const previous = mode;
    mode = next;
    updateDocumentTitle();
    root.classList.toggle("press-volumes-open", next === "volumes");
    root.classList.toggle("press-in-volumes", next === "volumes");
    // The clipped hero can acquire its own scroll offset when a browser brings
    // a shelf link into view. Its sticky canvas must stay at the viewport origin
    // across route changes; only window scrolling owns catalogue progression.
    if (hero) {
      hero.scrollTop = 0;
      hero.scrollLeft = 0;
    }
    updateDocumentHeight();
    updateAccess();
    if (next === "volumes") closeRailPreview();
    if (previous !== next) callbacks.onModeChange(next, previous, source);
  };

  const sectionScrollTop = (index: number): number => {
    const section = sections[index];
    return section ? section.getBoundingClientRect().top + window.scrollY : 0;
  };

  const scrollToVolume = (index: number): boolean => {
    if (!sections[index]) return false;
    instantScroll(sectionScrollTop(index));
    return true;
  };

  const goToVolume = (index: number, source: CleanRoomRouteSource): boolean => {
    if (!available || !sections[index]) return false;
    const enteringFromCatalogue = mode === "catalogue";
    callbacks.onBeforeVolume(index, source);
    const address = volumeAddress(index);
    // Stripe adds one history entry when leaving the product list, then
    // replaces that entry while the active product changes. Scrolling through
    // every slug therefore still has one Back step to the list.
    if (enteringFromCatalogue) history.pushState({}, "", address);
    else history.replaceState({}, "", address);
    currentAddress = address;
    setCurrentIndex(index, source);
    setMode("volumes", source);
    scrollToVolume(index);
    if (source !== "scroll") focusDestination(index);
    callbacks.onWake(1400);
    return true;
  };

  const restoreCataloguePosition = (index: number, source: CleanRoomRouteSource): void => {
    if (products[index]?.kind !== "book") {
      // Detail-page scroll is not terminal catalogue travel. Clear it before
      // the shelf is measured, then land on the requested terminal product.
      instantScroll(callbacks.catalogueScrollTop(Math.max(0, callbacks.links.length - 1)));
    }
    let landingPasses = 0;
    const restore = (): void => {
      catalogueRestoreFrame = 0;
      if (mode !== "catalogue") return;
      if (products[index]?.kind !== "book" && callbacks.catalogueReady?.() === false) {
        catalogueRestoreFrame = requestAnimationFrame(restore);
        return;
      }
      catalogueScrollY = callbacks.catalogueScrollTop(index);
      instantScroll(catalogueScrollY);
      callbacks.onWake(1800);
      // Route removal, shelf calibration, and document-height expansion land
      // in successive frames. Reapply through those passes before focusing;
      // the old, shorter document can clamp the first two scrolls.
      if (products[index]?.kind !== "book" && landingPasses++ < 3) {
        catalogueRestoreFrame = requestAnimationFrame(restore);
        return;
      }
      setCurrentIndex(index, source);
      focusDestination(index);
      callbacks.onWake(1800);
    };
    if (products[index]?.kind !== "book") {
      // Terminal positions depend on settled shelf calibration, not the
      // transient book poses used during the return flight.
      catalogueRestoreFrame = requestAnimationFrame(restore);
    } else {
      restore();
    }
  };

  const returnToCatalogue = (source: CleanRoomRouteSource): boolean => {
    if (!available || mode !== "volumes") return false;
    const index = currentIndex;
    callbacks.onBeforeCatalogue(index, source);
    history.pushState({}, "", catalogueAddress);
    currentAddress = catalogueAddress;
    setMode("catalogue", source);
    restoreCataloguePosition(index, source);
    return true;
  };

  const activate = (index: number, event?: MouseEvent): boolean => {
    if (event && (event.defaultPrevented || hasModifiedClick(event))) return false;
    if (!available) return false;
    event?.preventDefault();
    return goToVolume(index, "pick");
  };

  backButton?.addEventListener("click", (event) => {
    event.preventDefault();
    returnToCatalogue("back-control");
  });

  const catalogueBrand = document.querySelector<HTMLAnchorElement>(".home-brand");
  if (catalogueBrand && new URL(catalogueBrand.href).pathname === cataloguePath) {
    catalogueBrand.addEventListener("click", (event) => {
      if (event.defaultPrevented || hasModifiedClick(event) || !available) return;
      event.preventDefault();
      returnToCatalogue("back-control");
    });
  }

  filmLink?.addEventListener("click", (event) => {
    if (event.defaultPrevented || hasModifiedClick(event)) return;
    if (goToVolume(callbacks.links.length, "pick")) event.preventDefault();
  });

  signalLink?.addEventListener("click", (event) => {
    if (event.defaultPrevented || hasModifiedClick(event)) return;
    if (goToVolume(productLinks.indexOf(signalLink), "pick")) event.preventDefault();
  });

  railButtons.forEach((button) => button.setAttribute("aria-setsize", String(products.length)));

  railButtons.forEach((button, index) => {
    button.addEventListener("pointerenter", () => previewRailIndex(index));
    button.addEventListener("focus", () => previewRailIndex(index));
    button.addEventListener("click", (event) => {
      if (hasModifiedClick(event)) return;
      event.preventDefault();
      closeRailPreview();
      goToVolume(index, "rail");
    });
  });
  rail?.addEventListener("pointerleave", closeRailPreview);
  rail?.addEventListener("focusout", (event) => {
    if (!(event.relatedTarget instanceof Node) || !rail.contains(event.relatedTarget)) {
      closeRailPreview();
    }
  });

  window.addEventListener("keydown", (event) => {
    if (event.defaultPrevented || hasModifiedClick(event) || mode !== "volumes") return;
    if (document.querySelector("dialog[open]")) return;
    const target = event.target;
    if (target instanceof HTMLElement && (
      target.isContentEditable || /^(INPUT|TEXTAREA|SELECT)$/.test(target.tagName)
    )) return;
    if (event.key === "Escape") {
      event.preventDefault();
      returnToCatalogue("keyboard");
      return;
    }
    if (event.key !== "ArrowDown" && event.key !== "ArrowUp") return;
    const next = currentIndex + (event.key === "ArrowDown" ? 1 : -1);
    if (next < 0 || next >= sections.length) return;
    event.preventDefault();
    goToVolume(next, "keyboard");
  });

  window.addEventListener("popstate", () => {
    currentAddress = `${window.location.pathname}${window.location.search}${window.location.hash}`;
    const index = pathIndex();
    if (index >= 0 && sections[index]) {
      callbacks.onBeforeVolume(index, "popstate");
      setCurrentIndex(index, "popstate");
      setMode("volumes", "popstate");
      scrollToVolume(index);
      focusDestination(index);
      callbacks.onWake(1400);
      return;
    }
    const returningIndex = currentIndex;
    callbacks.onBeforeCatalogue(returningIndex, "popstate");
    setMode("catalogue", "popstate");
    restoreCataloguePosition(returningIndex, "popstate");
  });

  if (available) {
    const crossing = new Set<Element>();
    const observer = new IntersectionObserver((records) => {
      records.forEach((record) => {
        if (record.isIntersecting) crossing.add(record.target);
        else crossing.delete(record.target);
      });
      if (mode !== "volumes" || pendingDeepLinkIndex >= 0) return;
      const viewportCentre = window.innerHeight * 0.5;
      const section = sections
        .filter((candidate) => crossing.has(candidate))
        .sort((left, right) => {
          const leftRect = left.getBoundingClientRect();
          const rightRect = right.getBoundingClientRect();
          const leftDistance = Math.abs(leftRect.top + leftRect.height * 0.5 - viewportCentre);
          const rightDistance = Math.abs(rightRect.top + rightRect.height * 0.5 - viewportCentre);
          return leftDistance - rightDistance;
        })[0];
      if (!section) return;
      const index = sections.indexOf(section);
      const address = volumeAddress(index);
      if (address === currentAddress) return;
      currentAddress = address;
      history.replaceState({}, "", address);
      setCurrentIndex(index, "scroll");
      callbacks.onWake(500);
    }, { rootMargin: "-50% 0px -50% 0px", threshold: 0 });
    sections.forEach((section) => observer.observe(section));
  }

  const settlePendingDeepLink = (): boolean => {
    if (pendingDeepLinkIndex < 0) return false;
    const index = pendingDeepLinkIndex;
    pendingDeepLinkIndex = -1;
    updateDocumentHeight();
    scrollToVolume(index);
    callbacks.onWake(900);
    return true;
  };

  if (available) {
    history.scrollRestoration = "manual";
    if (deepLinkIndex >= 0) {
      history.replaceState({}, "", currentAddress);
      callbacks.onBeforeVolume(deepLinkIndex, "deep-link");
      setCurrentIndex(deepLinkIndex, "deep-link");
      setMode("volumes", "deep-link");
    } else {
      history.replaceState({}, "", currentAddress);
      pendingDeepLinkIndex = -1;
      setCurrentIndex(0, "deep-link");
      setMode("catalogue", "deep-link");
    }
  }

  return {
    sections,
    figures,
    available,
    snapshot: () => ({
      mode,
      productKind: products[currentIndex]?.kind ?? "book",
      currentIndex,
      currentAddress,
      pendingDeepLinkIndex,
      catalogueScrollY
    }),
    activate,
    returnToCatalogue,
    setCatalogueIndex: (index: number) => {
      if (mode === "catalogue") setCurrentIndex(index, "scroll");
    },
    refreshActiveInk: refreshActiveInks,
    settlePendingDeepLink
  };
};
