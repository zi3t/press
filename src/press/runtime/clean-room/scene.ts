// The page half of the Press scene. The 3D half (every book, the film poster
// and the signal medallion: their motion, camera, lights, projection and
// drawing) is the raylib module built from src/press/c into
// public/press/viewer/press.mjs. This file owns what only the page can do:
// the DOM (hit targets, terminal track, captions, classes), routing, scroll
// measurement and pointer capture. Each frame it writes the inputs the module
// reads (PRESS_INPUT) and applies what the module reports (PRESS_OUTPUT).
import posterArtwork from "../../assets/media/moving-pictures.svg?url";
import {
  cleanRoomCatalogueScrollStep,
  installCleanRoomCatalogueScroll
} from "./catalogue-scroll";
import {
  installCleanRoomInteraction,
  type CleanRoomInteractionSnapshot
} from "./interaction";
import { CLEAN_ROOM_MOTION, CLEAN_ROOM_REFERENCE, clamp, mix } from "./motion";
import { type CleanRoomVolumeProfile } from "./profiles";
import {
  installCleanRoomRouting,
  type CleanRoomPressMode
} from "./routing";
import {
  loadImagePixels,
  paintSurfaceTextures,
  PRESS_TEXTURE,
  type CleanRoomMetadata,
  type CleanRoomUpload
} from "./textures";
import { installCleanRoomVolumeInteraction } from "./volume-interaction";

const MODULE_URL = "/press/viewer/press.mjs";
const MAX_BOOKS = 8;

// PressInput in src/press/c/press.h, one double each.
const IN = {
  now: 0, dt: 1, width: 2, height: 3, dpr: 4, compact: 5, narrow: 6, reduced: 7,
  volumes: 8, currentRoute: 9, scrollY: 10, scrollStep: 11,
  terminalTravel: 12, terminalProgress: 13, scrollVelocity: 14, terminalSceneOpacity: 15,
  hover: 16, focus: 17, gesture: 18, gestureDx: 19, gestureDy: 20,
  volumeRotationX: 21, volumeRotationY: 22,
  figure: 23, // [valid, top, height] per book
  filmSourceTop: 23 + MAX_BOOKS * 3, posterPointerX: 24 + MAX_BOOKS * 3, posterPointerY: 25 + MAX_BOOKS * 3,
  signalTop: 26 + MAX_BOOKS * 3, signalBottom: 27 + MAX_BOOKS * 3, signalDetail: 28 + MAX_BOOKS * 3,
  signalFigureValid: 29 + MAX_BOOKS * 3, signalFigureLeft: 30 + MAX_BOOKS * 3, signalFigureTop: 31 + MAX_BOOKS * 3,
  signalFigureWidth: 32 + MAX_BOOKS * 3, signalFigureHeight: 33 + MAX_BOOKS * 3,
  signalPointerX: 34 + MAX_BOOKS * 3, signalPointerY: 35 + MAX_BOOKS * 3,
  count: 36 + MAX_BOOKS * 3
} as const;

// PressOutput in src/press/c/press.h.
const OUT = {
  presentedRoute: 0, events: 1, flightActive: 2, entryComplete: 3, holdingPresentation: 4,
  stackEvacuated: 5, signalAnimating: 6, idleReady: 7,
  backdropR: 8, backdropG: 9, backdropB: 10, backdropAlpha: 11,
  posterVisible: 12, posterLeft: 13, posterTop: 14, posterWidth: 15, posterHeight: 16,
  activeIndex: 17, posterSelectionReturning: 18, unitsPerPixel: 19, entryProgress: 20,
  count: 21
} as const;

const EVENT_ENTRY_COMPLETE = 1;
const EVENT_RETURN_SETTLED = 2;

interface ScreenBounds { left: number; top: number; width: number; height: number }

interface PressModule {
  readonly HEAPU8: Uint8Array;
  readonly HEAPF64: Float64Array;
  readonly FS: { writeFile(path: string, data: Uint8Array): void };
  _malloc(size: number): number;
  _free(pointer: number): void;
  _press_init(): number;
  _press_input(): number;
  _press_output(): number;
  _press_set_book(...args: number[]): void;
  _press_upload_texture(id: number, pointer: number, width: number, height: number): void;
  _press_layout(): number;
  _press_scroll(): void;
  _press_frame(): void;
  _press_render(): void;
  _press_gesture_begin(index: number): void;
  _press_gesture_finish(index: number, moved: number, now: number): void;
  _press_before_volume(index: number, deepLink: number): void;
  _press_before_catalogue(index: number): number;
  _press_mode_change(volumes: number): void;
  _press_index_change(index: number): void;
  _press_book_bounds(index: number): number;
  _press_book_bounds_at(index: number, cameraY: number): number;
  _press_camera_y_for_scroll(scrollY: number): number;
}

interface CleanRoomTerminalCalibration {
  readonly signatureHeight: number;
  readonly signatureWidth: number;
  readonly signatureBaseTop: number;
  readonly signatureStartTop: number;
  readonly signatureEndTop: number;
  readonly signalTop: number;
  readonly closingTop: number;
}

const getMetadata = (item: HTMLElement, index: number): CleanRoomMetadata => ({
  title: item.querySelector("strong")?.textContent?.trim() ?? "",
  eyebrow: item.querySelector("small")?.textContent?.trim() ?? "",
  serial: item.querySelector("b")?.textContent?.trim()
    ?? String(index + 1).padStart(2, "0")
});

const rgb = (hex: string): [number, number, number] => (
  [1, 3, 5].map((offset) => parseInt(hex.slice(offset, offset + 2), 16) / 255) as [number, number, number]
);

// signal.ts makeDisc: the three split faces of the medallion.
const paintDisc = (word: string, index: number): HTMLCanvasElement => {
  const canvas = document.createElement("canvas");
  canvas.width = canvas.height = 512;
  const context = canvas.getContext("2d")!;
  context.fillStyle = index === 1 ? "#676767" : "#353535";
  context.fillRect(0, 0, 512, 512);
  context.strokeStyle = index === 1 ? "#919191" : "#898989";
  context.lineWidth = 2;
  for (let n = 0; n < 22; n += 1) {
    context.beginPath();
    context.ellipse(256 + Math.sin(n * .8) * 28, 256, 24 + n * 15, 18 + n * 10, n * .07, 0, Math.PI * 2);
    context.stroke();
  }
  context.fillStyle = "#f2f2f2";
  context.font = "bold 66px Arial";
  context.textAlign = "center";
  context.textBaseline = "middle";
  context.fillText(word, 256, 256);
  return canvas;
};

export interface VolumeCatalogueOptions {
  readonly profiles: readonly CleanRoomVolumeProfile[];
}

export const mountCleanRoomCatalogue = async (
  options: VolumeCatalogueOptions
): Promise<boolean> => {
  const stage = document.querySelector<HTMLElement>(".press-catalog");
  const items = Array.from(document.querySelectorAll<HTMLElement>(".press-volume-item"));
  const profiles = options.profiles;
  if (!stage || items.length !== profiles.length || items.length > MAX_BOOKS) return false;
  const catalogueStage = stage;

  const links = items.map((item) => item.querySelector<HTMLAnchorElement>(".press-volume"));
  if (links.some((link) => !link)) return false;
  const ownedLinks = links as HTMLAnchorElement[];
  const volumeFigures = Array.from(document.querySelectorAll<HTMLElement>(".press-volume-figure"));
  const signaturePanel = document.querySelector<HTMLElement>(".signature-section");
  const signatureRow = signaturePanel?.querySelector<HTMLElement>(".signature-row");
  const filmLink = document.querySelector<HTMLAnchorElement>(".press-film-link");
  const filmHost = document.querySelector<HTMLElement>(".press-film-poster");
  const signalPanel = document.querySelector<HTMLElement>(".press-signal-section");
  const signalLink = document.querySelector<HTMLAnchorElement>(".press-signal-link");
  const signalDetail = document.querySelector<HTMLElement>(".press-signal-detail");
  const signalFigure = signalDetail?.querySelector<HTMLElement>(".press-volume-figure");
  const closingPanel = document.querySelector<HTMLElement>(".home-closing");
  const footerPanel = document.querySelector<HTMLElement>(".home-footer");

  const compact = window.matchMedia("(max-width: 899px)");
  const narrow = window.matchMedia("(max-width: 599px)");
  const reducedMotion = window.matchMedia("(prefers-reduced-motion: reduce)");
  // The reference only keeps Chromium's idle snapshot path on desktop; Safari
  // and compact layouts stay in a live render loop.
  const webkitCompositor = navigator.vendor === "Apple Computer, Inc.";
  const idlePauseAllowed = !compact.matches && !webkitCompositor;

  // ── the module ──────────────────────────────────────────────────────────
  const canvas = document.createElement("canvas");
  canvas.id = "press-canvas";  // raylib's web backend addresses its canvas by id
  canvas.className = "press-scene-canvas press-scene-canvas--clean-room";
  canvas.setAttribute("aria-hidden", "true");
  catalogueStage.prepend(canvas);
  // raylib's web backend (Emscripten GLFW) listens on the whole window and
  // cancels Tab and Backspace. Registered first, this keeps both keys away
  // from it, so page navigation keeps working.
  addEventListener("keydown", (event) => {
    if (event.key === "Tab" || event.key === "Backspace") event.stopImmediatePropagation();
  }, true);

  let press: PressModule;
  try {
    const { default: createPress } = await import(/* @vite-ignore */ MODULE_URL) as {
      default: (module: object) => Promise<PressModule>;
    };
    const Module = {
      canvas,
      print: () => undefined,
      printErr: (text: string) => console.warn(text)
    };
    press = await createPress(Module);
  } catch (error) {
    canvas.remove();
    console.warn("Press renderer unavailable; retaining the DOM catalogue.", error);
    return false;
  }

  const inputAt = press._press_input() / 8;
  const outputAt = press._press_output() / 8;
  const input = (): Float64Array => press.HEAPF64.subarray(inputAt, inputAt + IN.count);
  const output = (): Float64Array => press.HEAPF64.subarray(outputAt, outputAt + OUT.count);
  const readBounds = (pointer: number): ScreenBounds | null => {
    if (!pointer) return null;
    const at = pointer / 8;
    const [left, top, width, height] = press.HEAPF64.subarray(at, at + 4);
    return { left: left!, top: top!, width: width!, height: height! };
  };
  const upload: CleanRoomUpload = (id, pixels) => {
    const pointer = press._malloc(pixels.data.length);
    press.HEAPU8.set(pixels.data, pointer);
    press._press_upload_texture(id, pointer, pixels.width, pixels.height);
    press._free(pointer);
  };

  profiles.forEach((profile, index) => {
    const m = profile.material;
    press._press_set_book(index, profile.thickness, rgb(m.specular)[0], m.shininess, m.reflectiveness,
      rgb(m.diffuseBaseColor)[0], m.bump.base, m.bump.custom,
      ["none", "buckram", "paper", "cardboard"].indexOf(m.baseBump),
      m.foil.detail, m.foil.opacity, m.foil.specular, m.foil.emissive,
      m.gloss.opacity, m.gloss.specular, m.gloss.emissive,
      m.glitter.opacity, m.glitter.specular, m.glitter.emissive,
      ...rgb(profile.cloth));
  });

  let frameRequest = 0;
  let layoutFrame = 0;
  let lastFrameAt = 0;
  let startupRevealed = !document.documentElement.classList.contains("press-startup-pending");
  let filmSourceDocumentTop = 0;
  let renderUntil = performance.now() + 1800;
  let viewportWidth = window.innerWidth;
  let viewportHeight = window.innerHeight;
  let unitsPerPixel = 0.024;
  let holdClassTimer = 0;
  let posterPointerX = 0;
  let posterPointerY = 0;
  let signalPointerX = 0;
  let signalPointerY = 0;
  let signalVisible = false;
  const configuredRoutes = [...ownedLinks, ...(filmLink ? [filmLink] : []), ...(signalLink ? [signalLink] : [])]
    .map((link) => new URL(link.dataset.pressRoute ?? link.href, window.location.href).pathname);
  const initialRouteIndex = configuredRoutes.indexOf(window.location.pathname);
  let pressMode: CleanRoomPressMode = initialRouteIndex >= 0 ? "volumes" : "catalogue";
  let currentRouteIndex = Math.max(0, initialRouteIndex);
  let routeFrames = 0;
  let entryComplete = reducedMotion.matches;
  let requestRelayout = (): void => undefined;
  let invalidateScrollMeasurements = (): void => undefined;
  let resetVolumeInput = (): void => undefined;
  let terminalCalibration: CleanRoomTerminalCalibration | null = null;
  let appliedTerminalSignatureShift = "";
  let appliedTerminalSignalShift = "";
  let appliedTerminalClosingShift = "";
  let appliedFilmBounds: ScreenBounds | null = null;
  let scrollReady = false;
  let appliedEntryProgress = "";
  let interactionSnapshot: CleanRoomInteractionSnapshot = { hoverIndex: -1, focusIndex: -1, gesture: null };

  const holdCaption = document.createElement("aside");
  holdCaption.className = "press-hold-caption";
  holdCaption.setAttribute("aria-hidden", "true");
  catalogueStage.append(holdCaption);

  const writeViewport = (): void => {
    const values = input();
    values[IN.width] = viewportWidth;
    values[IN.height] = viewportHeight;
    values[IN.dpr] = window.devicePixelRatio || 1;
    values[IN.compact] = compact.matches ? 1 : 0;
    values[IN.narrow] = narrow.matches ? 1 : 0;
    values[IN.reduced] = reducedMotion.matches ? 1 : 0;
    values[IN.volumes] = pressMode === "volumes" ? 1 : 0;
    values[IN.currentRoute] = currentRouteIndex;
    values[IN.scrollY] = window.scrollY;
    // Routing can call back while it is still being installed (a deep link
    // opens its volume at once), before the scroll controller exists.
    values[IN.scrollStep] = scrollReady
      ? catalogueScroll.snapshot().currentScrollStep
      : cleanRoomCatalogueScrollStep(window.innerHeight, compact.matches);
  };

  const revealStartup = (): void => {
    if (startupRevealed) return;
    startupRevealed = true;
    document.documentElement.classList.remove("press-startup-pending");
    document.documentElement.classList.add("press-startup-ready");
  };
  const wake = (duration = 720): void => {
    renderUntil = Math.max(renderUntil, performance.now() + duration);
    if (!frameRequest) frameRequest = window.requestAnimationFrame(animate);
  };

  // Cover atlases, disc faces and the poster are painted by the page (fonts,
  // SVG and image decoding are the browser's). The book mesh and its shared
  // cloth, paper and glitter maps are generated inside the module.
  const loadTextures = async (): Promise<void> => {
    items.forEach((item, index) => {
      const profile = profiles[index]!;
      paintSurfaceTextures(index, profile, getMetadata(item, index), upload, () => wake(300));
    });
    ["SIGNAL", "IN", "MOTION"].forEach((word, index) => {
      const disc = paintDisc(word, index);
      upload(PRESS_TEXTURE.disc + index, disc.getContext("2d")!.getImageData(0, 0, 512, 512));
    });
    if (filmHost) {
      upload(PRESS_TEXTURE.poster, await loadImagePixels(posterArtwork));
      filmHost.classList.add("is-foldout-ready");
    }
    wake(300);
  };

  const bookBounds = (index: number): ScreenBounds | null => readBounds(press._press_book_bounds(index));

  // ── terminal track (DOM layout from projected book bounds) ──────────────
  const applyTerminalTrack = (terminalTravel: number): void => {
    const calibration = terminalCalibration;
    if (pressMode !== "catalogue" || !calibration || !signaturePanel || !signatureRow
      || !signalPanel || !closingPanel || !footerPanel) return;
    const catalogueMaximum = catalogueScroll.snapshot().currentScrollStep * Math.max(0, items.length - 1);
    const catalogueProgress = catalogueMaximum > 0
      ? clamp(Math.min(window.scrollY, catalogueMaximum) / catalogueMaximum, 0, 1)
      : 1;
    const signatureTop = mix(calibration.signatureStartTop, calibration.signatureEndTop, catalogueProgress) - terminalTravel;
    const signatureShift = signatureTop - calibration.signatureBaseTop;
    const signalShift = calibration.signalTop - terminalTravel;
    // Keep the final surface covering the viewport. Only measured overflowing
    // content may move it past the top; its background grows with that content.
    const closingOverflow = Math.max(0, (closingPanel.scrollHeight || viewportHeight) - viewportHeight);
    const closingShift = Math.max(-closingOverflow, calibration.closingTop - terminalTravel);
    const signatureValue = `${signatureShift.toFixed(2)}px`;
    const signalValue = `${signalShift.toFixed(2)}px`;
    const closingValue = `${closingShift.toFixed(2)}px`;
    if (signatureValue !== appliedTerminalSignatureShift) {
      appliedTerminalSignatureShift = signatureValue;
      signatureRow.style.transform = `translate3d(0, ${signatureValue}, 0)`;
    }
    if (signalValue !== appliedTerminalSignalShift) {
      appliedTerminalSignalShift = signalValue;
      signalPanel.style.transform = `translate3d(0, ${signalValue}, 0)`;
    }
    if (closingValue !== appliedTerminalClosingShift) {
      appliedTerminalClosingShift = closingValue;
      closingPanel.style.transform = `translate3d(0, ${closingValue}, 0)`;
      footerPanel.style.transform = `translate3d(0, ${closingValue}, 0)`;
    }
  };

  const calibrateTerminalLayout = (
    startBounds: ScreenBounds | null,
    endBounds: ScreenBounds | null,
    terminalTravel: number
  ): void => {
    if (pressMode !== "catalogue" || !entryComplete || !startBounds || !endBounds
      || !signaturePanel || !signatureRow || !signalPanel || !closingPanel || !footerPanel) {
      terminalCalibration = null;
      return;
    }
    const signatureHeight = Math.min(viewportHeight * 0.86, 760);
    const signatureWidth = signatureHeight * 3 / 4;
    const signatureBaseTop = (viewportHeight - signatureHeight) / 2;
    const signatureToSignalGap = Math.max(180, viewportHeight * 0.26);
    // The folded mesh is vertically contracted around its centre. Compensate
    // for that top inset so the visible paper begins at the authored gap.
    const foldedTopInset = signatureHeight * (1 - 0.68) * 0.5;
    const signatureStartTop = startBounds.top + terminalTravel + startBounds.height + 42 - foldedTopInset;
    const signatureEndTop = endBounds.top + terminalTravel + endBounds.height + 42 - foldedTopInset;
    const signalTop = signatureEndTop + signatureHeight + signatureToSignalGap;
    terminalCalibration = {
      signatureHeight,
      signatureWidth,
      signatureBaseTop,
      signatureStartTop,
      signatureEndTop,
      signalTop,
      closingTop: signalTop + viewportHeight
    };
    applyTerminalTrack(terminalTravel);
  };

  const lastIndex = items.length - 1;
  const projectedTerminalBookBounds = (): ScreenBounds | null => {
    const catalogueMaximum = catalogueScroll.snapshot().currentScrollStep * Math.max(0, items.length - 1);
    return readBounds(press._press_book_bounds_at(lastIndex, press._press_camera_y_for_scroll(catalogueMaximum)));
  };
  const projectedInitialBookBounds = (): ScreenBounds | null => (
    readBounds(press._press_book_bounds_at(lastIndex, CLEAN_ROOM_REFERENCE.cameraY))
  );

  const syncInteractionBounds = (recalibrateTerminal = true): void => {
    if (pressMode !== "catalogue") return;
    const itemRects = items.map((item) => item.getBoundingClientRect());
    items.forEach((_, index) => {
      const itemRect = itemRects[index];
      const bounds = bookBounds(index);
      const link = ownedLinks[index]!;
      if (!itemRect || !bounds) return;
      link.style.setProperty("--press-hit-left", `${(bounds.left - itemRect.left).toFixed(2)}px`);
      link.style.setProperty("--press-hit-top", `${(bounds.top - itemRect.top).toFixed(2)}px`);
      link.style.setProperty("--press-hit-width", `${bounds.width.toFixed(2)}px`);
      link.style.setProperty("--press-hit-height", `${bounds.height.toFixed(2)}px`);
    });
    if (recalibrateTerminal) {
      calibrateTerminalLayout(projectedInitialBookBounds(), projectedTerminalBookBounds(),
        catalogueScroll.snapshot().terminalTravel);
    }
  };

  const clearHoldClasses = (immediate = false): void => {
    window.clearTimeout(holdClassTimer);
    const remove = (): void => {
      catalogueStage.classList.remove("is-book-held", "is-book-dragging");
      document.body.classList.remove("press-book-held", "press-book-dragging");
    };
    if (immediate) remove();
    else holdClassTimer = window.setTimeout(remove, CLEAN_ROOM_MOTION.releaseClassDelay);
  };

  const catalogueProductScrollTop = (index: number): number => {
    const bookTop = cleanRoomCatalogueScrollStep(window.innerHeight, compact.matches)
      * Math.max(0, Math.min(items.length - 1, index));
    if (index > items.length && signalPanel) {
      if (compact.matches || reducedMotion.matches) {
        return Math.max(0, signalPanel.getBoundingClientRect().top + window.scrollY);
      }
      return bookTop + (terminalCalibration?.signalTop ?? viewportHeight * 1.6);
    }
    if (index < items.length || !filmLink) return bookTop;
    if (compact.matches || reducedMotion.matches) {
      return Math.max(0, (signaturePanel?.getBoundingClientRect().top ?? 0) + window.scrollY);
    }
    // Film is the next product, with its own catalogue landing position.
    return bookTop + Math.max(0, (terminalCalibration?.signatureEndTop ?? viewportHeight * 0.5)
      + (terminalCalibration?.signatureHeight ?? viewportHeight * 0.86) * 0.5
      - viewportHeight * 0.5);
  };

  const routing = installCleanRoomRouting({
    items,
    links: ownedLinks,
    catalogueScrollTop: catalogueProductScrollTop,
    catalogueReady: () => !output()[OUT.flightActive],
    onBeforeVolume: (index, source) => {
      clearHoldClasses(true);
      resetVolumeInput();
      writeViewport();
      press._press_before_volume(index, source === "deep-link" ? 1 : 0);
    },
    onBeforeCatalogue: (index) => {
      resetVolumeInput();
      writeViewport();
      const animateReturn = press._press_before_catalogue(index) !== 0;
      document.documentElement.classList.toggle("press-catalogue-returning", animateReturn);
    },
    onModeChange: (mode) => {
      pressMode = mode;
      routeFrames = 0;
      invalidateScrollMeasurements();
      press._press_mode_change(mode === "volumes" ? 1 : 0);
      if (mode === "volumes") {
        entryComplete = true;
        document.documentElement.classList.add("press-entry-complete");
      }
      requestRelayout();
      wake(1800);
    },
    onIndexChange: (index) => {
      if (index !== currentRouteIndex) resetVolumeInput();
      currentRouteIndex = index;
      press._press_index_change(index);
      if (pressMode === "volumes") requestRelayout();
      wake(900);
    },
    onWake: wake
  });
  pressMode = routing.snapshot().mode;
  currentRouteIndex = routing.snapshot().currentIndex;
  let pendingAbout = false;
  document.addEventListener("press:about", () => {
    routing.returnToCatalogue("keyboard");
    pendingAbout = true;
    wake(1800);
  });

  const catalogueScroll = installCleanRoomCatalogueScroll({
    items,
    stage: catalogueStage,
    compact,
    reducedMotion,
    closingStart: () => terminalCalibration?.closingTop ?? window.innerHeight * 2.42,
    terminalExtent: () => terminalCalibration
      ? terminalCalibration.closingTop + Math.max(0, (closingPanel?.scrollHeight ?? window.innerHeight) - window.innerHeight)
      : window.innerHeight * CLEAN_ROOM_MOTION.terminalScrollViewports,
    filmScrollTop: () => catalogueProductScrollTop(items.length),
    signalScrollTop: () => catalogueProductScrollTop(items.length + 1),
    mode: () => pressMode,
    onCurrentIndex: routing.setCatalogueIndex
  });
  invalidateScrollMeasurements = catalogueScroll.invalidateMeasurements;
  scrollReady = true;

  const volumeInteraction = installCleanRoomVolumeInteraction({
    figures: routing.figures.slice(0, items.length),
    canInteract: () => (
      pressMode === "volumes"
      && currentRouteIndex < items.length
      && !compact.matches
      && !reducedMotion.matches
      && !output()[OUT.flightActive]
    ),
    flightActive: () => Boolean(output()[OUT.flightActive]),
    onWake: wake
  });
  resetVolumeInput = volumeInteraction.reset;

  const interactionState = installCleanRoomInteraction({
    items,
    links: ownedLinks,
    canHold: () => pressMode === "catalogue" && !compact.matches && !reducedMotion.matches,
    onBegin: (gesture) => {
      clearHoldClasses(true);
      press._press_gesture_begin(gesture.index);
      const profile = profiles[gesture.index];
      if (!profile) return;
      document.body.style.setProperty("--press-held-background", profile.cloth);
      document.body.style.setProperty("--press-held-ink", profile.ink);
      holdCaption.textContent = profile.caption;
      catalogueStage.classList.add("is-book-held");
      document.body.classList.add("press-book-held");
    },
    onMove: (_, startedDragging) => {
      if (startedDragging) {
        catalogueStage.classList.add("is-book-dragging");
        document.body.classList.add("press-book-dragging");
      }
    },
    onFinish: (gesture) => {
      press._press_gesture_finish(gesture.index, gesture.moved ? 1 : 0, performance.now());
      catalogueStage.classList.remove("is-book-dragging");
      document.body.classList.remove("press-book-dragging");
      clearHoldClasses(false);
    },
    onHover: () => undefined,
    onFocus: () => undefined,
    onActivate: (index, event) => {
      routing.activate(index, event);
    },
    onWake: wake
  });

  // The film poster and the signal medallion tilt toward the pointer.
  window.addEventListener("mousemove", (event) => {
    if (!output()[OUT.posterVisible]) return;
    const nextX = event.clientX - window.innerWidth * 0.5;
    const nextY = event.clientY - window.innerHeight * 0.5;
    if (nextX === posterPointerX && nextY === posterPointerY) return;
    posterPointerX = nextX;
    posterPointerY = nextY;
    wake();
  }, { passive: true });
  window.addEventListener("pointermove", (event) => {
    if (!signalVisible || reducedMotion.matches || event.pointerType === "touch") return;
    const x = event.clientX / viewportWidth - .5;
    const y = event.clientY / viewportHeight - .5;
    if (x === signalPointerX && y === signalPointerY) return;
    signalPointerX = x;
    signalPointerY = y;
    wake();
  }, { passive: true });
  window.addEventListener("blur", () => { signalPointerX = signalPointerY = 0; });

  // ── layout ──────────────────────────────────────────────────────────────
  function layout(): void {
    layoutFrame = 0;
    viewportWidth = window.innerWidth;
    viewportHeight = window.innerHeight;
    catalogueScroll.update();
    writeViewport();
    // Sticky figures: read their live rectangles together on the layout pass.
    const values = input();
    items.forEach((_, index) => {
      const rect = pressMode === "volumes" ? volumeFigures[index]?.getBoundingClientRect() : null;
      const at = IN.figure + index * 3;
      values[at] = rect ? 1 : 0;
      values[at + 1] = rect?.top ?? 0;
      values[at + 2] = rect?.height ?? 0;
    });
    const ranEntryLayout = press._press_layout() !== 0;
    unitsPerPixel = output()[OUT.unitsPerPixel] || unitsPerPixel;
    if (ranEntryLayout && pressMode === "catalogue") {
      const filmSource = items[Math.max(0, items.length - 2)];
      const filmSourceRect = filmSource?.getBoundingClientRect();
      if (filmSourceRect) filmSourceDocumentTop = filmSourceRect.top + window.scrollY;
    }
    const layoutScroll = catalogueScroll.snapshot();
    if (pressMode === "catalogue" && layoutScroll.terminalProgress > 0) {
      calibrateTerminalLayout(projectedInitialBookBounds(), projectedTerminalBookBounds(), layoutScroll.terminalTravel);
    } else {
      syncInteractionBounds();
    }
    wake(240);
  }

  const scheduleLayout = (): void => {
    if (layoutFrame) return;
    layoutFrame = window.requestAnimationFrame(layout);
  };
  requestRelayout = scheduleLayout;

  // ── animate ─────────────────────────────────────────────────────────────
  function animate(now: number): void {
    frameRequest = 0;
    const rawDelta = lastFrameAt ? (now - lastFrameAt) / 1000 : 1 / 60;
    const deltaSeconds = clamp(rawDelta, 1 / 240, 0.25);
    lastFrameAt = now;
    interactionSnapshot = interactionState();
    const catalogueMotion = catalogueScroll.advance(deltaSeconds);
    // Sticky detail geometry and the terminal panel own the signal's window.
    const signalRect = pressMode === "volumes" ? signalDetail?.getBoundingClientRect() : signalPanel?.getBoundingClientRect();
    const figureRect = pressMode === "volumes" ? signalFigure?.getBoundingClientRect() ?? null : null;
    if (pressMode === "volumes") volumeInteraction.advanceTwirl(deltaSeconds);
    const volumePose = volumeInteraction.snapshot();
    const flying = Boolean(output()[OUT.flightActive]);
    const returningFlight = flying && document.documentElement.classList.contains("press-catalogue-returning");

    writeViewport();
    const values = input();
    values[IN.now] = now;
    values[IN.dt] = deltaSeconds;
    values[IN.terminalTravel] = catalogueMotion.terminalTravel;
    values[IN.terminalProgress] = catalogueMotion.terminalProgress;
    values[IN.scrollVelocity] = catalogueMotion.scrollVelocity;
    values[IN.terminalSceneOpacity] = catalogueMotion.terminalSceneOpacity;
    const gesture = interactionSnapshot.gesture;
    values[IN.hover] = interactionSnapshot.hoverIndex;
    values[IN.focus] = interactionSnapshot.focusIndex;
    values[IN.gesture] = gesture?.index ?? -1;
    values[IN.gestureDx] = gesture?.dx ?? 0;
    values[IN.gestureDy] = gesture?.dy ?? 0;
    values[IN.volumeRotationX] = volumePose.rotationX;
    values[IN.volumeRotationY] = volumePose.rotationY;
    values[IN.filmSourceTop] = filmSourceDocumentTop;
    values[IN.posterPointerX] = posterPointerX;
    values[IN.posterPointerY] = posterPointerY;
    values[IN.signalTop] = returningFlight ? viewportHeight : signalRect?.top ?? viewportHeight;
    values[IN.signalBottom] = returningFlight ? viewportHeight : signalRect?.bottom ?? viewportHeight;
    values[IN.signalDetail] = pressMode === "volumes" ? 1 : 0;
    values[IN.signalFigureValid] = figureRect ? 1 : 0;
    values[IN.signalFigureLeft] = figureRect?.left ?? 0;
    values[IN.signalFigureTop] = figureRect?.top ?? 0;
    values[IN.signalFigureWidth] = figureRect?.width ?? 0;
    values[IN.signalFigureHeight] = figureRect?.height ?? 0;
    values[IN.signalPointerX] = signalPointerX;
    values[IN.signalPointerY] = signalPointerY;
    press._press_frame();

    const out = output();
    const events = out[OUT.events]!;
    // The rail and help arrive with the books, not after them: CSS paces their
    // stagger from this and both finish on the frame the shelf settles.
    const entryProgress = out[OUT.entryProgress]!.toFixed(3);
    if (entryProgress !== appliedEntryProgress) {
      appliedEntryProgress = entryProgress;
      document.documentElement.style.setProperty("--press-entry-progress", entryProgress);
    }
    const holding = Boolean(gesture);
    const flightActive = Boolean(out[OUT.flightActive]);
    signalVisible = Boolean(out[OUT.signalAnimating]);
    document.documentElement.classList.toggle("press-signal-visible", signalVisible);
    catalogueStage.classList.toggle("is-stack-evacuated", Boolean(out[OUT.stackEvacuated]));

    if (filmLink && signatureRow && !compact.matches && !reducedMotion.matches) {
      const bounds = out[OUT.posterVisible]
        ? { left: out[OUT.posterLeft]!, top: out[OUT.posterTop]!, width: out[OUT.posterWidth]!, height: out[OUT.posterHeight]! }
        : null;
      const live = pressMode === "catalogue" && !holding && !out[OUT.posterSelectionReturning] && !flightActive
        && Boolean(bounds && bounds.top < viewportHeight && bounds.top + bounds.height > 0);
      filmLink.inert = !live;
      filmLink.style.visibility = live ? "visible" : "hidden";
      const changed = !appliedFilmBounds || !bounds || appliedFilmBounds.left !== bounds.left
        || appliedFilmBounds.top !== bounds.top || appliedFilmBounds.width !== bounds.width
        || appliedFilmBounds.height !== bounds.height;
      if (bounds && live && changed) {
        appliedFilmBounds = bounds;
        const rowRect = signatureRow.getBoundingClientRect();
        filmLink.style.setProperty("--press-film-hit-left", `${bounds.left - rowRect.left}px`);
        filmLink.style.setProperty("--press-film-hit-top", `${bounds.top - rowRect.top}px`);
        filmLink.style.setProperty("--press-film-hit-width", `${bounds.width}px`);
        filmLink.style.setProperty("--press-film-hit-height", `${bounds.height}px`);
      }
      if (!live) appliedFilmBounds = null;
    } else if (filmLink) {
      filmLink.style.removeProperty("visibility");
      filmLink.inert = pressMode !== "catalogue";
    }

    if (events & EVENT_RETURN_SETTLED) {
      // Measure only the settled shelf, never a book still flying home.
      document.documentElement.classList.remove("press-catalogue-returning");
      requestRelayout();
    }
    if (events & EVENT_ENTRY_COMPLETE) {
      entryComplete = true;
      // Hit targets stay inert while the books fly in; measure the settled
      // shelf before they become clickable, then anchor the terminal track
      // to the final pose.
      syncInteractionBounds(false);
      document.documentElement.classList.add("press-entry-complete");
      requestRelayout();
    }

    const activeIndex = out[OUT.activeIndex]!;
    if (out[OUT.holdingPresentation] && activeIndex >= 0) {
      const bounds = bookBounds(activeIndex);
      if (bounds) {
        const height = holdCaption.offsetHeight;
        const upper = viewportHeight * 0.201;
        const lower = Math.max(upper, viewportHeight - height - viewportHeight * 0.12);
        const gap = 32;
        const overlap = (top: number): number => Math.max(0,
          Math.min(top + height, bounds.top + bounds.height + gap) - Math.max(top, bounds.top - gap));
        const preferLower = bounds.top + bounds.height * 0.5 < viewportHeight * 0.5;
        const preferred = preferLower ? lower : upper;
        const alternate = preferLower ? upper : lower;
        const top = overlap(preferred) <= overlap(alternate) ? preferred : alternate;
        holdCaption.style.setProperty("--press-caption-top", `${top.toFixed(1)}px`);
      }
    }

    if (pendingAbout && !flightActive && !layoutFrame && entryComplete) {
      pendingAbout = false;
      const top = catalogueScroll.snapshot().currentScrollStep * Math.max(0, items.length - 1)
        + (terminalCalibration?.closingTop ?? viewportHeight * 2.42);
      window.scrollTo({ top, behavior: "instant" });
      closingPanel?.querySelector<HTMLElement>("[data-press-about-top]")?.focus({ preventScroll: true });
      wake(500);
    }
    const latestVolumePose = volumeInteraction.snapshot();
    const idlePaused = idlePauseAllowed
      && Boolean(out[OUT.idleReady])
      && !latestVolumePose.dragging
      && !latestVolumePose.twirlX
      && !latestVolumePose.twirlY
      && routing.snapshot().pendingDeepLinkIndex < 0
      && now > renderUntil + CLEAN_ROOM_MOTION.idlePauseAfter;
    if (document.visibilityState !== "hidden" && !idlePaused) {
      // Hit targets describe this rendered frame; keep capture stable during a
      // hold and never measure a route flight.
      if (entryComplete && !flightActive && !gesture) syncInteractionBounds(false);
      press._press_render();
      const readyToReveal = pressMode === "catalogue" ? true : routing.snapshot().pendingDeepLinkIndex < 0;
      if (readyToReveal) revealStartup();
    }
    routeFrames += 1;
    if (routeFrames >= 3 && routing.settlePendingDeepLink()) {
      routeFrames = 0;
      requestRelayout();
    }
    if (!idlePaused && !frameRequest) frameRequest = window.requestAnimationFrame(animate);
  }

  const handleScroll = (): void => {
    if (pressMode === "volumes") {
      scheduleLayout();
      return;
    }
    const scroll = catalogueScroll.update();
    writeViewport();
    press._press_scroll();
    applyTerminalTrack(scroll.terminalTravel);
    wake(240);
  };

  window.addEventListener("resize", scheduleLayout, { passive: true });
  window.addEventListener("scroll", handleScroll, { passive: true });
  compact.addEventListener("change", scheduleLayout);
  narrow.addEventListener("change", scheduleLayout);
  reducedMotion.addEventListener("change", scheduleLayout);

  // Mode and route reach the module through PRESS_INPUT on every layout and
  // frame; a deep-linked volume settles its entry on the first frame.
  writeViewport();
  if (!press._press_init()) {
    canvas.remove();
    console.warn("Press renderer failed to initialise; retaining the DOM catalogue.");
    return false;
  }
  void loadTextures();
  layout();
  document.documentElement.dataset.pressRenderer = "clean-room";
  document.documentElement.classList.add("press-signal-enhanced");
  // Refresh the shared menu token once the clean-room selectors resolve.
  routing.refreshActiveInk();
  document.documentElement.classList.remove("press-entry-complete");
  document.documentElement.classList.add("press-scene-ready");
  if (entryComplete) document.documentElement.classList.add("press-entry-complete");

  wake(1800);
  return true;
};
