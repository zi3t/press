import { CLEAN_ROOM_MOTION, clamp, wrapRotation } from "./motion";

export interface CleanRoomVolumeInteractionSnapshot {
  readonly rotationX: number;
  readonly rotationY: number;
  readonly dragging: boolean;
  readonly twirlX: number;
  readonly twirlY: number;
}

interface CleanRoomVolumeInteractionCallbacks {
  readonly figures: readonly (HTMLElement | null)[];
  readonly canInteract: () => boolean;
  readonly flightActive: () => boolean;
  readonly onWake: (duration?: number) => void;
}

export interface CleanRoomVolumeInteractionController {
  readonly snapshot: () => CleanRoomVolumeInteractionSnapshot;
  readonly reset: () => void;
  readonly advanceTwirl: (deltaSeconds: number) => boolean;
}

interface ModifiedPointerEvent {
  readonly button: number;
  readonly metaKey: boolean;
  readonly ctrlKey: boolean;
  readonly shiftKey: boolean;
  readonly altKey: boolean;
}

const modified = (event: ModifiedPointerEvent): boolean => (
  event.button !== 0
  || event.metaKey
  || event.ctrlKey
  || event.shiftKey
  || event.altKey
);

const shortestRotationDelta = (current: number, previous: number): number => (
  Math.atan2(Math.sin(current - previous), Math.cos(current - previous))
);

export const installCleanRoomVolumeInteraction = (
  callbacks: CleanRoomVolumeInteractionCallbacks
): CleanRoomVolumeInteractionController => {
  let rotationX = 0;
  let rotationY = 0;
  let lastX = 0;
  let lastY = 0;
  let baseX = 0;
  let baseY = 0;
  let anchorX = 0;
  let anchorY = 0;
  let dragging = false;
  let pointerId = -1;
  let captureTarget: HTMLElement | null = null;
  let twirlX = 0;
  let twirlY = 0;
  let horizontalDirection = 1;
  let pointerX = 0;
  let pointerY = 0;

  const point = (event: PointerEvent): { x: number; y: number } => ({
    x: event.clientX - window.innerWidth * 0.5,
    y: event.clientY - window.innerHeight * 0.5
  });

  const reset = (): void => {
    const previousTarget = captureTarget;
    const previousPointer = pointerId;
    rotationX = 0;
    rotationY = 0;
    lastX = 0;
    lastY = 0;
    baseX = 0;
    baseY = 0;
    anchorX = 0;
    anchorY = 0;
    dragging = false;
    pointerId = -1;
    captureTarget = null;
    try {
      if (previousTarget?.hasPointerCapture(previousPointer)) {
        previousTarget.releasePointerCapture(previousPointer);
      }
    } catch {
      // Capture may already have been released by the browser.
    }
    twirlX = 0;
    twirlY = 0;
    horizontalDirection = 1;
    document.body.classList.remove("press-cover-dragging");
  };

  const begin = (event: PointerEvent): void => {
    if (
      !callbacks.canInteract()
      || dragging
      || callbacks.flightActive()
      || event.pointerType === "touch"
      || modified(event)
    ) return;
    const current = point(event);
    event.preventDefault();
    pointerX = current.x;
    pointerY = current.y;
    dragging = true;
    pointerId = event.pointerId;
    captureTarget = event.currentTarget instanceof HTMLElement ? event.currentTarget : null;
    anchorX = current.x;
    anchorY = current.y;
    baseX = rotationX;
    baseY = rotationY;
    lastX = rotationX;
    lastY = rotationY;
    twirlX = 0;
    twirlY = 0;
    const absoluteX = Math.abs(wrapRotation(rotationX));
    horizontalDirection = absoluteX > Math.PI / 2
      && absoluteX < Math.PI * 1.5
      ? -1
      : 1;
    try {
      captureTarget?.setPointerCapture(pointerId);
    } catch {
      // Window listeners still own the drag when capture is unavailable.
    }
    document.body.classList.add("press-cover-dragging");
    callbacks.onWake(900);
  };

  const move = (event: PointerEvent): void => {
    // A released cover belongs to inertia, not the hovering cursor.
    if (!dragging) return;
    if (dragging && event.pointerId !== pointerId) return;
    if (dragging && !callbacks.canInteract()) {
      cancel();
      return;
    }
    if (dragging && (event.buttons & 1) === 0) {
      finish(event);
      return;
    }
    if (!callbacks.canInteract() || event.pointerType === "touch") return;
    const current = point(event);
    pointerX = current.x;
    pointerY = current.y;
    const rate = CLEAN_ROOM_MOTION.volumeDragRate;
    lastX = rotationX;
    lastY = rotationY;
    rotationX = wrapRotation((current.y - anchorY) * rate + baseX);
    rotationY = wrapRotation(
      (current.x - anchorX) * rate * horizontalDirection + baseY
    );
    callbacks.onWake(500);
  };

  const finish = (event: PointerEvent): void => {
    if (!dragging || event.pointerId !== pointerId) return;
    dragging = false;
    try {
      if (captureTarget?.hasPointerCapture(pointerId)) {
        captureTarget.releasePointerCapture(pointerId);
      }
    } catch {
      // Capture may already be released at a window edge.
    }
    const current = point(event);
    anchorX = current.x;
    anchorY = current.y;
    baseX = rotationX;
    baseY = rotationY;
    twirlX = clamp(
      shortestRotationDelta(rotationX, lastX),
      -CLEAN_ROOM_MOTION.volumeTwirlLimit,
      CLEAN_ROOM_MOTION.volumeTwirlLimit
    );
    twirlY = clamp(
      shortestRotationDelta(rotationY, lastY),
      -CLEAN_ROOM_MOTION.volumeTwirlLimit,
      CLEAN_ROOM_MOTION.volumeTwirlLimit
    );
    pointerId = -1;
    captureTarget = null;
    document.body.classList.remove("press-cover-dragging");
    callbacks.onWake(1800);
  };

  const cancel = (): void => {
    if (!dragging && !twirlX && !twirlY) return;
    // Losing the window/pointer ends the gesture, not the book's pose.
    // Only an explicit route/mode reset is allowed to restore the front cover.
    const target = captureTarget;
    const id = pointerId;
    dragging = false;
    pointerId = -1;
    captureTarget = null;
    try {
      if (target?.hasPointerCapture(id)) target.releasePointerCapture(id);
    } catch {
      // A cancelled pointer may already have lost capture.
    }
    anchorX = pointerX;
    anchorY = pointerY;
    baseX = rotationX;
    baseY = rotationY;
    twirlX = twirlY = 0;
    document.body.classList.remove("press-cover-dragging");
    callbacks.onWake(500);
  };
  const cancelPointer = (event: PointerEvent): void => {
    if (dragging && event.pointerId === pointerId) cancel();
  };

  callbacks.figures.forEach((figure) => {
    figure?.addEventListener("pointerdown", begin);
    figure?.addEventListener("dragstart", (event) => event.preventDefault());
    figure?.addEventListener("lostpointercapture", (event) => {
      // Capture is an optimization, not ownership of the gesture. Continue
      // through the window listeners when the pointer leaves the figure or
      // capture is transferred; pointerup/cancel/blur still end it safely.
      if (dragging && event.pointerId === pointerId) captureTarget = null;
    });
  });
  window.addEventListener("pointermove", move, { capture: true, passive: true });
  window.addEventListener("pointerup", finish, { capture: true, passive: true });
  window.addEventListener("pointercancel", cancelPointer, { capture: true, passive: true });
  window.addEventListener("blur", cancel);
  document.addEventListener("visibilitychange", () => {
    if (document.visibilityState === "hidden") cancel();
  });

  const advanceTwirl = (deltaSeconds: number): boolean => {
    if (!twirlX && !twirlY) return false;
    const frameCount = clamp(deltaSeconds * 60, 0.25, 4);
    const decay = Math.pow(
      CLEAN_ROOM_MOTION.volumeTwirlDecay,
      frameCount
    );
    const advance = (1 - decay) / (1 - CLEAN_ROOM_MOTION.volumeTwirlDecay);
    rotationX = wrapRotation(rotationX + twirlX * advance);
    rotationY = wrapRotation(rotationY + twirlY * advance);
    baseX = rotationX;
    baseY = rotationY;
    twirlX *= decay;
    twirlY *= decay;
    if (Math.abs(twirlX) + Math.abs(twirlY) < 0.001) {
      twirlX = 0;
      twirlY = 0;
    } else {
      callbacks.onWake(120);
    }
    return true;
  };

  return {
    snapshot: () => ({ rotationX, rotationY, dragging, twirlX, twirlY }),
    reset,
    advanceTwirl
  };
};
