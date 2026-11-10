import type { CleanRoomVolumeProfile } from "./profiles";

export interface CleanRoomMetadata {
  readonly title: string;
  readonly eyebrow: string;
  readonly serial: string;
}

/**
 * Hands RGBA8 pixels (top row first) to the raylib Press module, which
 * mipmaps, filters and orients them for its meshes.
 */
export type CleanRoomUpload = (id: number, pixels: ImageData) => void;

// Texture ids shared with src/press/c/press.h (0-5 are generated there).
export const PRESS_TEXTURE = {
  poster: 6,
  disc: 7,
  book: (index: number, map: 0 | 1 | 2 | 3) => 16 + index * 4 + map
} as const;

interface AtlasCanvases {
  readonly diffuse: HTMLCanvasElement;
  readonly customBump: HTMLCanvasElement;
  readonly foil: HTMLCanvasElement;
  readonly gloss: HTMLCanvasElement;
}

const ATLAS_WIDTH = 1920;
const ATLAS_HEIGHT = 1600;
// Rows above 320 hold the page edges, pastedowns, endband and foil tile; the
// cloth wrap starts there. The generated mesh (c/press_book.c) maps into
// exactly these rectangles.
const WRAP_TOP = 320;
const SPINE_LEFT = 874;
const SPINE_RIGHT = 1046;
const FRONT_LEFT = 1040;
// Same stack as the site's --sans: SF Pro on Apple devices.
const FONT = '-apple-system, BlinkMacSystemFont, "SF Pro Text", "SF Pro", system-ui, "Helvetica Neue", Helvetica, Arial, sans-serif';
const FOIL_TILE_WIDTH = Math.round(ATLAS_WIDTH * 0.14);
const FOIL_TILE_HEIGHT = Math.round(ATLAS_HEIGHT * 0.19);
// The generated mesh maps its endbands onto this swatch. Keep ink inside
// these bounds: the page edges sit beside it, so an oversized swatch prints
// across the visible fore edge.
const ENDBAND_LEFT = 421;
const ENDBAND_TOP = 197;
const ENDBAND_WIDTH = 195;
const ENDBAND_HEIGHT = 52;
const ENDBAND_STITCH_COUNT = 12;

const pixelsOf = (source: CanvasImageSource, width: number, height: number): ImageData => {
  const canvas = document.createElement("canvas");
  canvas.width = width;
  canvas.height = height;
  const context = canvas.getContext("2d", { willReadFrequently: true });
  if (!context) throw new Error("2D canvas is unavailable for texture upload");
  context.drawImage(source, 0, 0, width, height);
  return context.getImageData(0, 0, width, height);
};

const canvasPixels = (canvas: HTMLCanvasElement): ImageData => {
  const context = canvas.getContext("2d");
  if (!context) throw new Error("2D canvas is unavailable for texture upload");
  return context.getImageData(0, 0, canvas.width, canvas.height);
};

// Decodes an image through the browser at its natural size.
export const loadImagePixels = async (url: string, width?: number, height?: number): Promise<ImageData> => {
  const image = new Image();
  image.decoding = "async";
  image.src = url;
  await image.decode();
  return pixelsOf(image, width ?? image.naturalWidth, height ?? image.naturalHeight);
};

const createCanvas = (): HTMLCanvasElement => {
  const canvas = document.createElement("canvas");
  canvas.width = ATLAS_WIDTH;
  canvas.height = ATLAS_HEIGHT;
  return canvas;
};

const createAtlasCanvases = (): AtlasCanvases => ({
  diffuse: createCanvas(),
  customBump: createCanvas(),
  foil: createCanvas(),
  gloss: createCanvas()
});

const contextFor = (
  canvas: HTMLCanvasElement,
  label: string
): CanvasRenderingContext2D => {
  // Read back for every upload to the raylib module.
  const context = canvas.getContext("2d", { alpha: false, willReadFrequently: true });
  if (!context) throw new Error(`2D canvas is unavailable for ${label}`);
  return context;
};

const reset = (
  context: CanvasRenderingContext2D,
  color: string
): void => {
  context.setTransform(1, 0, 0, 1, 0, 0);
  context.globalAlpha = 1;
  context.globalCompositeOperation = "source-over";
  context.fillStyle = color;
  context.fillRect(0, 0, ATLAS_WIDTH, ATLAS_HEIGHT);
};

const paintFoilPalette = (
  context: CanvasRenderingContext2D,
  colors: readonly [string, string]
): void => {
  const gradient = context.createLinearGradient(0, 0, FOIL_TILE_WIDTH, FOIL_TILE_HEIGHT);
  gradient.addColorStop(0, colors[0]);
  gradient.addColorStop(0.35, colors[1]);
  gradient.addColorStop(0.7, colors[0]);
  gradient.addColorStop(1, colors[1]);
  context.fillStyle = gradient;
  context.fillRect(0, 0, FOIL_TILE_WIDTH, FOIL_TILE_HEIGHT);
};

const paintEndband = (
  context: CanvasRenderingContext2D,
  color: string
): void => {
  const pitch = ENDBAND_WIDTH / ENDBAND_STITCH_COUNT;
  context.save();
  context.fillStyle = color;
  for (let index = 0; index < ENDBAND_STITCH_COUNT; index += 1) {
    const x = ENDBAND_LEFT + index * pitch;
    const top = ENDBAND_TOP + (index % 3 === 1 ? 1 : 0);
    const bottom = ENDBAND_TOP + ENDBAND_HEIGHT - (index % 4 === 2 ? 1 : 0);
    context.beginPath();
    context.moveTo(x + 2, top);
    context.lineTo(x + pitch * 0.72, top + 2);
    context.lineTo(x + pitch * 0.58, top + ENDBAND_HEIGHT * 0.36);
    context.lineTo(x + pitch * 0.76, top + ENDBAND_HEIGHT * 0.58);
    context.lineTo(x + pitch * 0.55, bottom);
    context.lineTo(x, bottom - 2);
    context.lineTo(x + pitch * 0.12, top + ENDBAND_HEIGHT * 0.62);
    context.lineTo(x - 1, top + ENDBAND_HEIGHT * 0.34);
    context.closePath();
    context.fill();
  }
  context.restore();
};

const wrapText = (
  context: CanvasRenderingContext2D,
  text: string,
  centerX: number,
  startY: number,
  maxWidth: number,
  lineHeight: number
): void => {
  const words = text.split(/\s+/);
  const lines: string[] = [];
  let line = "";
  words.forEach((word) => {
    const candidate = line ? `${line} ${word}` : word;
    if (line && context.measureText(candidate).width > maxWidth) {
      lines.push(line);
      line = word;
    } else {
      line = candidate;
    }
  });
  if (line) lines.push(line);
  lines.forEach((value, index) => {
    context.fillText(value, centerX, startY + index * lineHeight, maxWidth);
  });
};

const paintFrontTypography = (
  context: CanvasRenderingContext2D,
  metadata: CleanRoomMetadata,
  color: string
): void => {
  const left = FRONT_LEFT + 72;
  context.fillStyle = color;
  context.textAlign = "left";
  context.textBaseline = "alphabetic";
  context.font = `400 23px ${FONT}`;
  context.globalAlpha = 0.7;
  context.fillText(metadata.eyebrow.toLowerCase(), left, WRAP_TOP + 92, 700);
  context.font = `400 52px ${FONT}`;
  context.globalAlpha = 1;
  context.fillText(metadata.title, left, WRAP_TOP + 160, 720);
};

const paintBackTypography = (
  context: CanvasRenderingContext2D,
  profile: CleanRoomVolumeProfile
): void => {
  context.save();
  context.fillStyle = profile.ink;
  context.textAlign = "center";
  context.textBaseline = "alphabetic";
  context.globalAlpha = 0.92;
  context.font = `400 36px ${FONT}`;
  wrapText(context, profile.caption, 430, 760, 560, 46);
  context.font = `400 18px ${FONT}`;
  context.globalAlpha = 0.72;
  context.fillText(`zi3t / ${profile.slug}`, 430, 1470);
  context.restore();
};

const paintSpineTypography = (
  context: CanvasRenderingContext2D,
  profile: CleanRoomVolumeProfile,
  metadata: CleanRoomMetadata,
  color: string
): void => {
  const centerX = (SPINE_LEFT + SPINE_RIGHT) * 0.5;
  const centerY = (WRAP_TOP + ATLAS_HEIGHT) * 0.5;
  context.save();
  context.translate(centerX, centerY);
  context.rotate(Math.PI / 2);
  const length = ATLAS_HEIGHT - WRAP_TOP;
  context.fillStyle = color;
  context.textBaseline = "middle";
  context.font = `400 22px ${FONT}`;
  context.textAlign = "left";
  context.globalAlpha = 0.72;
  context.fillText(metadata.eyebrow.toLowerCase(), -length * 0.44, 0, length * 0.22);
  context.font = `400 49px ${FONT}`;
  context.textAlign = "center";
  context.globalAlpha = 1;
  context.fillText(metadata.title, profile.spineNote ? -70 : 0, 0, length * 0.45);
  if (profile.spineNote) {
    context.font = `400 26px ${FONT}`;
    context.globalAlpha = 0.82;
    context.fillText(profile.spineNote, length * 0.3, 0, length * 0.18);
  }
  context.textAlign = "right";
  context.font = `400 19px ${FONT}`;
  context.globalAlpha = 0.72;
  context.fillText(metadata.serial, length * 0.45, 0);
  context.restore();
};

const paintArtwork = (
  context: CanvasRenderingContext2D,
  artwork: HTMLImageElement
): void => {
  context.drawImage(
    artwork,
    FRONT_LEFT,
    WRAP_TOP,
    ATLAS_WIDTH - FRONT_LEFT,
    ATLAS_HEIGHT - WRAP_TOP
  );
};

const createArtworkMask = (
  artwork: HTMLImageElement,
  color = "#ffffff"
): HTMLCanvasElement => {
  const canvas = createCanvas();
  const context = canvas.getContext("2d");
  if (!context) throw new Error("2D canvas is unavailable for the artwork mask");
  paintArtwork(context, artwork);
  context.globalCompositeOperation = "source-in";
  context.fillStyle = color;
  context.fillRect(0, 0, ATLAS_WIDTH, ATLAS_HEIGHT);
  return canvas;
};

const paintAtlas = (
  canvases: AtlasCanvases,
  profile: CleanRoomVolumeProfile,
  metadata: CleanRoomMetadata,
  artwork: HTMLImageElement | null
): void => {
  const diffuse = contextFor(canvases.diffuse, "the unified diffuse atlas");
  reset(diffuse, profile.paper);
  diffuse.fillStyle = profile.cloth;
  diffuse.fillRect(0, WRAP_TOP, ATLAS_WIDTH, ATLAS_HEIGHT - WRAP_TOP);
  paintFoilPalette(diffuse, profile.material.foil.colors);
  paintEndband(diffuse, profile.headband[0]);
  if (artwork) paintArtwork(diffuse, artwork);
  paintBackTypography(diffuse, profile);
  paintFrontTypography(diffuse, metadata, profile.ink);
  paintSpineTypography(diffuse, profile, metadata, profile.ink);

  const bump = contextFor(canvases.customBump, "the unified custom-bump atlas");
  const foil = contextFor(canvases.foil, "the unified foil atlas");
  const gloss = contextFor(canvases.gloss, "the unified gloss atlas");
  reset(bump, "#808080");
  reset(foil, "#000000");
  reset(gloss, "#000000");

  if (artwork) {
    const bumpInk = profile.material.bump.custom < 0 ? "#000000" : "#ffffff";
    const bumpMask = createArtworkMask(artwork, bumpInk);
    const foilMask = createArtworkMask(artwork);
    bump.save();
    bump.globalAlpha = 0.9;
    bump.drawImage(bumpMask, 0, 0);
    bump.restore();
    foil.save();
    foil.globalAlpha = 0.92;
    foil.drawImage(foilMask, 0, 0);
    foil.restore();
  }

  const bumpInk = profile.material.bump.custom < 0 ? "#000000" : "#ffffff";
  paintFrontTypography(bump, metadata, bumpInk);
  paintSpineTypography(bump, profile, metadata, bumpInk);
  paintFrontTypography(foil, metadata, "#ffffff");
  paintSpineTypography(foil, profile, metadata, "#ffffff");
};

/**
 * Paints the volume's four atlases (diffuse, custom bump, foil, gloss) and
 * uploads them; repaints once the artwork and again once the pixel font load.
 */
export const paintSurfaceTextures = (
  index: number,
  profile: CleanRoomVolumeProfile,
  metadata: CleanRoomMetadata,
  upload: CleanRoomUpload,
  invalidate: () => void
): void => {
  const canvases = createAtlasCanvases();
  const send = (): void => {
    upload(PRESS_TEXTURE.book(index, 0), canvasPixels(canvases.diffuse));
    upload(PRESS_TEXTURE.book(index, 1), canvasPixels(canvases.customBump));
    upload(PRESS_TEXTURE.book(index, 2), canvasPixels(canvases.foil));
    upload(PRESS_TEXTURE.book(index, 3), canvasPixels(canvases.gloss));
    invalidate();
  };
  paintAtlas(canvases, profile, metadata, null);
  send();

  const artwork = new Image();
  artwork.decoding = "async";
  artwork.addEventListener("load", () => {
    paintAtlas(canvases, profile, metadata, artwork);
    send();
  }, { once: true });
  artwork.src = profile.artworkUrl;
};
