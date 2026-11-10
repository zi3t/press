import { CLEAN_ROOM_MOTION } from "./motion";

export interface CleanRoomHoldGesture {
  readonly pointerId: number;
  readonly index: number;
  readonly startX: number;
  readonly startY: number;
  readonly captureTarget: HTMLElement;
  dx: number;
  dy: number;
  moved: boolean;
}

export interface CleanRoomInteractionSnapshot {
  readonly hoverIndex: number;
  readonly focusIndex: number;
  readonly gesture: CleanRoomHoldGesture | null;
}

interface CleanRoomInteractionCallbacks {
  readonly items: readonly HTMLElement[];
  readonly links: readonly HTMLElement[];
  readonly canHold: () => boolean;
  readonly onBegin: (gesture: CleanRoomHoldGesture) => void;
  readonly onMove: (gesture: CleanRoomHoldGesture, startedDragging: boolean) => void;
  readonly onFinish: (gesture: CleanRoomHoldGesture) => void;
  readonly onHover: (index: number) => void;
  readonly onFocus: (index: number) => void;
  readonly onActivate: (index: number, event: MouseEvent) => void;
  readonly onWake: () => void;
}

const hasModifiedClick = (event: MouseEvent): boolean => (
  event.button > 0
  || event.metaKey
  || event.ctrlKey
  || event.shiftKey
  || event.altKey
);

export const installCleanRoomInteraction = (
  callbacks: CleanRoomInteractionCallbacks
): (() => CleanRoomInteractionSnapshot) => {
  let hoverIndex = -1;
  let focusIndex = -1;
  // Focus only presents a book (pulls it off the shelf) while the keyboard is
  // driving. Routing focuses the returned book's link for accessibility; a
  // mouse user would otherwise find that book stuck forward, ignoring hover,
  // until something else took focus.
  let keyboardDriving = true;
  let gesture: CleanRoomHoldGesture | null = null;
  let completedGesture: Pick<CleanRoomHoldGesture, "index" | "pointerId" | "moved"> | null = null;

  const begin = (index: number, event: PointerEvent): void => {
    if (
      event.pointerType !== "mouse"
      || event.button !== 0
      || hasModifiedClick(event)
      || !callbacks.canHold()
    ) return;

    const captureTarget = event.currentTarget instanceof HTMLElement
      ? event.currentTarget
      : callbacks.items[index];
    if (!captureTarget) return;

    completedGesture = null;
    hoverIndex = index;
    gesture = {
      pointerId: event.pointerId,
      index,
      startX: event.clientX,
      startY: event.clientY,
      captureTarget,
      dx: 0,
      dy: 0,
      moved: false
    };
    try {
      captureTarget.setPointerCapture(event.pointerId);
    } catch {
      // Window capture remains authoritative when a browser declines capture.
    }
    callbacks.onBegin(gesture);
    callbacks.onWake();
  };

  const move = (event: PointerEvent): void => {
    if (!gesture || gesture.pointerId !== event.pointerId) return;
    if ((event.buttons & 1) === 0 || !callbacks.canHold()) {
      cancel();
      return;
    }
    gesture.dx = event.clientX - gesture.startX;
    gesture.dy = event.clientY - gesture.startY;
    const startedDragging = !gesture.moved
      && Math.abs(gesture.dx) + Math.abs(gesture.dy) > CLEAN_ROOM_MOTION.dragThreshold;
    if (startedDragging) gesture.moved = true;
    callbacks.onMove(gesture, startedDragging);
    callbacks.onWake();
  };

  const complete = (cancelled: boolean): void => {
    if (!gesture) return;
    const finished = gesture;
    gesture = null;
    if (finished.captureTarget.hasPointerCapture(finished.pointerId)) {
      try {
        finished.captureTarget.releasePointerCapture(finished.pointerId);
      } catch {
        // Capture may already be gone after a window-edge release.
      }
    }
    completedGesture = {
      index: finished.index,
      pointerId: finished.pointerId,
      moved: cancelled || finished.moved
    };
    hoverIndex = -1;
    callbacks.onFinish(finished);
    callbacks.onWake();
  };
  const cancel = (): void => complete(true);
  const finish = (event: PointerEvent): void => {
    if (gesture?.pointerId === event.pointerId) complete(false);
  };
  const cancelPointer = (event: PointerEvent): void => {
    if (gesture?.pointerId === event.pointerId) cancel();
  };

  callbacks.items.forEach((item, index) => {
    const link = callbacks.links[index];
    const interactionTarget = link ?? item;
    interactionTarget.addEventListener("pointerdown", (event) => begin(index, event));
    interactionTarget.addEventListener("lostpointercapture", cancelPointer);
    interactionTarget.addEventListener("pointerenter", () => {
      hoverIndex = index;
      callbacks.onHover(index);
      callbacks.onWake();
    });
    interactionTarget.addEventListener("pointerleave", () => {
      if (gesture?.index === index) return;
      if (hoverIndex === index) hoverIndex = -1;
      callbacks.onHover(hoverIndex);
      callbacks.onWake();
    });
    interactionTarget.addEventListener("click", (event) => {
      const completed = event.detail > 0 && completedGesture?.index === index
        ? completedGesture
        : null;
      if (completed) completedGesture = null;
      if (completed?.moved || (gesture?.index === index && gesture.moved)) {
        event.preventDefault();
        event.stopPropagation();
        return;
      }
      callbacks.onActivate(index, event);
    });
    link?.addEventListener("dragstart", (event) => event.preventDefault());
    link?.addEventListener("focus", () => {
      focusIndex = index;
      callbacks.onFocus(index);
      callbacks.onWake();
    });
    link?.addEventListener("blur", () => {
      if (focusIndex === index) focusIndex = -1;
      callbacks.onFocus(focusIndex);
      callbacks.onWake();
    });
  });

  const pointerDriving = (): void => {
    if (!keyboardDriving) return;
    keyboardDriving = false;
    if (focusIndex >= 0) callbacks.onWake();
  };
  window.addEventListener("pointermove", pointerDriving, { capture: true, passive: true });
  window.addEventListener("pointerdown", pointerDriving, { capture: true, passive: true });
  window.addEventListener("keydown", () => {
    if (keyboardDriving) return;
    keyboardDriving = true;
    if (focusIndex >= 0) callbacks.onWake();
  }, true);
  window.addEventListener("pointermove", move, { capture: true, passive: true });
  window.addEventListener("pointerup", finish, true);
  window.addEventListener("pointercancel", cancelPointer, true);
  window.addEventListener("blur", cancel);
  document.addEventListener("visibilitychange", () => {
    if (document.visibilityState === "hidden") cancel();
  });

  return () => ({ hoverIndex, focusIndex: keyboardDriving ? focusIndex : -1, gesture });
};
