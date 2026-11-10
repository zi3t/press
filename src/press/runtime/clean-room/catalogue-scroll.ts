import { CLEAN_ROOM_MOTION, clamp, mix, smooth } from "./motion";
import type { CleanRoomPressMode } from "./routing";

export interface CleanRoomCatalogueScrollSnapshot {
  readonly currentIndex: number;
  readonly currentScrollStep: number;
  readonly stackShift: number;
  readonly scrollVelocity: number;
  readonly terminalProgress: number;
  readonly terminalTravel: number;
  readonly terminalSceneOpacity: number;
  readonly mainHeight: number;
}

interface CleanRoomCatalogueScrollOptions {
  readonly items: readonly HTMLElement[];
  readonly stage: HTMLElement;
  readonly compact: MediaQueryList;
  readonly reducedMotion: MediaQueryList;
  readonly mode: () => CleanRoomPressMode;
  readonly terminalExtent?: () => number;
  readonly closingStart?: () => number;
  readonly filmScrollTop?: () => number;
  readonly signalScrollTop?: () => number;
  readonly onCurrentIndex: (index: number) => void;
}

export interface CleanRoomCatalogueScrollController {
  update: () => CleanRoomCatalogueScrollSnapshot;
  advance: (deltaSeconds: number) => CleanRoomCatalogueScrollSnapshot;
  invalidateMeasurements: () => void;
  snapshot: () => CleanRoomCatalogueScrollSnapshot;
}

const emptySnapshot = (): CleanRoomCatalogueScrollSnapshot => ({
  currentIndex: 0,
  currentScrollStep: 1,
  stackShift: 0,
  scrollVelocity: 0,
  terminalProgress: 0,
  terminalTravel: 0,
  terminalSceneOpacity: 1,
  mainHeight: 0
});

export const cleanRoomCatalogueScrollStep = (
  viewportHeight: number,
  compact: boolean
): number => viewportHeight * (compact ? 0.225 : 0.213);

export const installCleanRoomCatalogueScroll = (
  options: CleanRoomCatalogueScrollOptions
): CleanRoomCatalogueScrollController => {
  const main = document.querySelector<HTMLElement>(".home-page main");
  const list = document.querySelector<HTMLElement>(".press-volume-list");
  const volumes = document.querySelector<HTMLElement>(".press-volumes");
  const signaturePanel = document.querySelector<HTMLElement>(".signature-section");
  const signalPanel = document.querySelector<HTMLElement>(".press-signal-section");
  const closingPanel = document.querySelector<HTMLElement>(".home-closing");
  const footerPanel = document.querySelector<HTMLElement>(".home-footer");
  const rail = document.querySelector<HTMLElement>(".press-rail");
  const help = document.querySelector<HTMLElement>(".press-help");
  let state = emptySnapshot();
  let lastScrollY = window.scrollY;
  let reportedIndex = -1;
  let measuredViewportWidth = -1;
  let measuredViewportHeight = -1;
  let measuredCompact = options.compact.matches;
  let itemOffsets: number[] = [];
  let measuredVolumeHeight = 0;
  let appliedMainHeight = "";
  let appliedStackShift = "";
  let appliedTerminalOpacity = "";
  let appliedControlsOpacity = "";
  let appliedTerminalActive = false;
  let appliedTerminalClosing = false;
  let terminalBookClipApplied = false;
  let accessMode: CleanRoomPressMode | null = null;
  let accessTerminal = false;
  const accessibleStates = new WeakMap<HTMLElement, string>();
  let appliedStaticShelfTravel = "";

  const setStaticShelfTravel = (value: string): void => {
    if (value === appliedStaticShelfTravel) return;
    appliedStaticShelfTravel = value;
    main?.style.setProperty("--press-static-shelf-travel", value);
  };

  const terminalExtent = (): number => Math.max(0,
    options.terminalExtent?.() ?? window.innerHeight * CLEAN_ROOM_MOTION.terminalScrollViewports
  );
  const setAccessible = (element: HTMLElement | null, visible: boolean, interactive = visible): void => {
    if (!element) return;
    const key = `${visible}:${interactive}`;
    if (accessibleStates.get(element) === key) return;
    accessibleStates.set(element, key);
    element.inert = !interactive;
    element.setAttribute("aria-hidden", String(!interactive));
    element.style.visibility = visible ? "" : "hidden";
  };

  document.body.classList.remove("press-terminal-active", "press-terminal-closing");
  if (closingPanel) closingPanel.inert = true;
  if (footerPanel) footerPanel.inert = true;

  const updateTerminal = (progress: number): void => {
    const terminalProgress = clamp(progress, 0, 1);
    const terminalTravel = terminalProgress * terminalExtent();
    const terminalScreens = terminalTravel / Math.max(1, window.innerHeight);
    // The reference keeps the outgoing object and the next scene on one
    // continuous vertical track. The film poster starts immediately after the
    // fifth book. The signal environment and light closing surface follow it
    // on that same one-pixel-per-pixel track.
    const terminalActive = terminalTravel > 0.5;
    const terminalOpacity = smooth(clamp(terminalScreens / 0.06, 0, 1));
    const terminalSceneOpacity = 1;

    const opacityValue = terminalOpacity.toFixed(4);
    if (opacityValue !== appliedTerminalOpacity) {
      appliedTerminalOpacity = opacityValue;
      if (signaturePanel) signaturePanel.style.setProperty("--press-terminal-opacity", opacityValue);
      if (signalPanel) signalPanel.style.opacity = opacityValue;
      if (closingPanel) closingPanel.style.opacity = opacityValue;
      if (footerPanel) footerPanel.style.opacity = opacityValue;
    }
    if (!terminalBookClipApplied) {
      terminalBookClipApplied = true;
      options.stage.style.setProperty("--press-terminal-book-clip", "0%");
    }
    const inCatalogue = options.mode() === "catalogue";
    const staticCatalogue = options.compact.matches || options.reducedMotion.matches;
    const closingStart = options.closingStart?.() ?? window.innerHeight * 2.42;
    const controlsOpacity = 1 - smooth(clamp(
      (terminalTravel - closingStart + window.innerHeight * 0.34)
        / Math.max(1, window.innerHeight * 0.34), 0, 1
    ));
    const controlsValue = controlsOpacity.toFixed(4);
    if (controlsValue !== appliedControlsOpacity) {
      appliedControlsOpacity = controlsValue;
      if (rail) rail.style.opacity = controlsValue;
      if (help) help.style.opacity = controlsValue;
    }

    const active = terminalActive;
    // Keep the masthead and controls in their dark-surface colours while the
    // light page is only entering at the bottom. They switch only when that
    // surface reaches the fixed header, as on the reference.
    const closing = inCatalogue && !staticCatalogue && terminalTravel >= closingStart;
    if (active !== appliedTerminalActive) {
      appliedTerminalActive = active;
      document.body.classList.toggle("press-terminal-active", active);
    }
    if (closing !== appliedTerminalClosing) {
      appliedTerminalClosing = closing;
      document.body.classList.toggle("press-terminal-closing", closing);
    }
    setAccessible(rail, controlsOpacity > 0.01);
    setAccessible(help, controlsOpacity > 0.01);
    // The projected poster can enter before terminal travel begins. Its link
    // owns viewport/gesture gating; this ancestor only owns route visibility.
    setAccessible(signaturePanel, inCatalogue);
    setAccessible(signalPanel, inCatalogue && (staticCatalogue || terminalActive));
    const closingVisible = inCatalogue && !staticCatalogue && terminalActive;
    const closingInteractive = closingVisible && terminalTravel > closingStart - window.innerHeight;
    setAccessible(closingPanel, closingVisible, closingInteractive);
    setAccessible(footerPanel, closingVisible, closingInteractive);
    state = { ...state, terminalProgress, terminalTravel, terminalSceneOpacity };
  };

  const updateAccess = (mode: CleanRoomPressMode): void => {
    const terminal = state.terminalProgress > 0.04;
    if (mode === accessMode && terminal === accessTerminal) return;
    accessMode = mode;
    accessTerminal = terminal;
    options.items.forEach((item) => {
      item.inert = mode === "volumes" || terminal;
    });
  };

  const setMainHeight = (height: string): void => {
    if (!main || height === appliedMainHeight) return;
    appliedMainHeight = height;
    if (height) main.style.height = height;
    else main.style.removeProperty("height");
  };

  const setStackShift = (value: string): void => {
    if (!list || value === appliedStackShift) return;
    appliedStackShift = value;
    // `--press-stack-shift` is inherited. Updating it per pixel makes Safari
    // restyle the entire invisible hit-target subtree before every frame. A
    // direct transform stays on the list's compositor layer and preserves the
    // exact same screen-space alignment without descendant invalidation.
    list.style.transform = `translate3d(
      calc(-50% + var(--press-shift-x)),
      calc(var(--press-shift-y) + ${value} + var(--press-scroll-y)),
      0
    )`;
  };

  const measureLayout = (): void => {
    const width = window.innerWidth;
    const height = window.innerHeight;
    const compact = options.compact.matches;
    if (
      width === measuredViewportWidth
      && height === measuredViewportHeight
      && compact === measuredCompact
      && itemOffsets.length === options.items.length
    ) return;
    measuredViewportWidth = width;
    measuredViewportHeight = height;
    measuredCompact = compact;
    itemOffsets = options.items.map((item) => item.offsetTop);
    measuredVolumeHeight = volumes?.offsetHeight ?? 0;
  };

  const reportCurrentIndex = (index: number): void => {
    if (index === reportedIndex) return;
    reportedIndex = index;
    options.onCurrentIndex(index);
  };

  const update = (): CleanRoomCatalogueScrollSnapshot => {
    const mode = options.mode();
    const nextScrollY = window.scrollY;
    const scrollDelta = lastScrollY - nextScrollY;
    lastScrollY = nextScrollY;

    if (!main || !list || options.reducedMotion.matches) {
      setStaticShelfTravel("0px");
      setMainHeight("");
      setStackShift("0px");
      state = {
        ...state,
        currentIndex: 0,
        currentScrollStep: 1,
        stackShift: 0,
        scrollVelocity: 0,
        mainHeight: main?.offsetHeight ?? 0
      };
      updateTerminal(0);
      if (mode === "catalogue") reportCurrentIndex(0);
      updateAccess(mode);
      return state;
    }

    const compact = options.compact.matches;
    const currentScrollStep = cleanRoomCatalogueScrollStep(
      window.innerHeight,
      compact
    );
    measureLayout();
    if (mode === "volumes") {
      const volumeMainHeight = Math.ceil(measuredVolumeHeight);
      setMainHeight(`${volumeMainHeight}px`);
      setStackShift("0px");
      state = {
        ...state,
        currentScrollStep,
        stackShift: 0,
        scrollVelocity: 0,
        mainHeight: volumeMainHeight
      };
      updateTerminal(0);
      updateAccess(mode);
      return state;
    }

    if (Math.abs(scrollDelta) > 0.01) {
      state = {
        ...state,
        scrollVelocity: clamp(
          scrollDelta * CLEAN_ROOM_MOTION.scrollVelocityPerPixel,
          -CLEAN_ROOM_MOTION.scrollVelocityLimit,
          CLEAN_ROOM_MOTION.scrollVelocityLimit
        )
      };
    }
    const catalogueMaximum = currentScrollStep * Math.max(0, options.items.length - 1);
    setStaticShelfTravel(compact ? `${catalogueMaximum}px` : "0px");
    const terminalLength = compact
      ? 0
      : terminalExtent();
    const catalogueMainHeight = Math.round(
      Math.max(window.innerHeight, compact ? 640 : 0) + catalogueMaximum + terminalLength
        + (compact ? (signaturePanel?.offsetHeight ?? 0) + (signalPanel?.offsetHeight ?? 0) : 0)
    );
    setMainHeight(`${catalogueMainHeight}px`);
    const catalogueScroll = Math.min(window.scrollY, catalogueMaximum);
    const floatingIndex = clamp(
      catalogueScroll / currentScrollStep,
      0,
      Math.max(0, options.items.length - 1)
    );
    const lower = Math.floor(floatingIndex);
    const upper = Math.min(options.items.length - 1, lower + 1);
    const local = floatingIndex - lower;
    const lowerOffset = itemOffsets[lower] ?? 0;
    const upperOffset = itemOffsets[upper] ?? lowerOffset;
    const stackShift = mix(lowerOffset, upperOffset, local);
    const filmTop = options.filmScrollTop?.() ?? Number.POSITIVE_INFINITY;
    const signalTop = options.signalScrollTop?.() ?? Number.POSITIVE_INFINITY;
    const currentIndex = window.scrollY >= signalTop - currentScrollStep * 0.5
      ? options.items.length + 1
      : window.scrollY >= filmTop - currentScrollStep * 0.5
        ? options.items.length
        : Math.round(floatingIndex);
    setStackShift(`${-stackShift.toFixed(2)}px`);
    state = {
      ...state,
      currentIndex,
      currentScrollStep,
      stackShift,
      mainHeight: catalogueMainHeight
    };
    reportCurrentIndex(currentIndex);
    updateTerminal(
      compact || terminalLength <= 0
        ? 0
        : (window.scrollY - catalogueMaximum) / terminalLength
    );
    updateAccess(mode);
    return state;
  };

  const advance = (deltaSeconds: number): CleanRoomCatalogueScrollSnapshot => {
    const frames = clamp(deltaSeconds * 60, 0.25, 4);
    const velocity = state.scrollVelocity
      * Math.pow(CLEAN_ROOM_MOTION.scrollVelocityDecay, frames);
    state = {
      ...state,
      scrollVelocity: Math.abs(velocity) < 0.0001 ? 0 : velocity
    };
    return state;
  };

  return {
    update,
    advance,
    invalidateMeasurements: () => {
      measuredViewportWidth = -1;
      measuredViewportHeight = -1;
    },
    snapshot: () => state
  };
};
