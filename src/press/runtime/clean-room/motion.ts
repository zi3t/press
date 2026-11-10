/** Values read from the current Stripe Press camera and shared book scene. */
export const CLEAN_ROOM_REFERENCE = {
  canvasMaxWidth: 2000,
  canvasReferenceHeight: 1018,
  canvasOverscan: 1.1,
  positionReferenceWidth: 1792,
  cameraBaseZ: 100,
  cameraY: 6.5,
  cameraPitch: -0.06,
  catalogueCameraScroll: 0.0222,
  modelScale: 1,
  shelfRootZ: -3,
  // Stripe pulls the hovered root toward z=6, then its ordinary shelf
  // transform simultaneously pulls it back toward z=-3. Their observed
  // equilibrium is ~z=1.5; using that effective target avoids inventing a
  // second feedback loop while preserving the same projected motion.
  shelfHoverZ: 1.5,
  shelfCoverOffsetX: 11,
  shelfRootRotationX: -(Math.PI / 2),
  shelfRootRotationZ: Math.PI / 2,
  coverBaseRotationY: -(Math.PI / 2),
  activeX: -13,
  activeY: -4,
  activeZ: -56,
  inactiveZ: -50,
  activeRotationX: -0.5,
  activeRotationY: 0.35,
  activeRotationZ: 0.15,
  compactActiveX: 0,
  compactActiveY: 3,
  compactActiveZ: -90,
  narrowActivePitchOffset: 0.16
} as const;

export const CLEAN_ROOM_MOTION = {
  dragThreshold: 4,
  entrySettleEpsilon: 0.012,
  entryInitialY: 3,
  entryInitialGap: 3,
  entryInitialZ: -50,
  entryInitialDepthArc: 150,
  stackEvictionViewports: 1.12,
  spineZEase: 0.1,
  releaseIsolation: 360,
  releasePresentation: 110,
  releaseBackdrop: 430,
  releaseClassDelay: 780,
  flightEaseStep: 0.006,
  flightEaseCeiling: 0.15,
  volumeFollowRate: 0.00015,
  volumeDragRate: 0.003,
  volumeScrollTurn: 0.0008,
  volumeTwirlLimit: 0.3,
  volumeTwirlDecay: 0.95,
  scrollVelocityPerPixel: 0.003,
  scrollVelocityDecay: 0.4,
  scrollVelocityLimit: 1,
  heldLiftPixels: 0,
  terminalScrollViewports: 3.36,
  idlePauseAfter: 1200
} as const;

export const clamp = (value: number, minimum: number, maximum: number): number => (
  Math.min(maximum, Math.max(minimum, value))
);

export const mix = (from: number, to: number, progress: number): number => (
  from + (to - from) * progress
);

export const damp = (
  from: number,
  to: number,
  speed: number,
  deltaSeconds: number
): number => mix(from, to, 1 - Math.exp(-speed * deltaSeconds));

export const frameApproach = (
  from: number,
  to: number,
  perFrame: number,
  deltaSeconds: number
): number => mix(
  from,
  to,
  1 - Math.pow(1 - perFrame, clamp(deltaSeconds * 60, 0.25, 4))
);

export const smooth = (progress: number): number => {
  const value = clamp(progress, 0, 1);
  return value * value * (3 - 2 * value);
};

export const wrapRotation = (radians: number): number => radians % (Math.PI * 2);
