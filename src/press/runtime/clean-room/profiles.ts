import geometryArtwork from "../../assets/geometry-cover.svg?url";
import integrationArtwork from "../../assets/integration-cover.svg?url";
import routingArtwork from "../../assets/routing-cover.svg?url";
import surfacesArtwork from "../../assets/surfaces-cover.svg?url";
import interactionArtwork from "../../assets/interaction-cover.svg?url";

export type CleanRoomBaseBump = "none" | "buckram" | "paper" | "cardboard";

export interface CleanRoomMaterialProfile {
  readonly shininess: number;
  readonly specular: string;
  readonly reflectiveness: number;
  readonly diffuseBaseColor: string;
  readonly baseBump: CleanRoomBaseBump;
  readonly bump: {
    readonly base: number;
    readonly custom: number;
  };
  readonly foil: {
    /** The shader samples these colors from the atlas's foil-palette tile. */
    readonly colors: readonly [string, string];
    readonly detail: number;
    readonly opacity: number;
    readonly specular: number;
    readonly emissive: number;
  };
  readonly gloss: {
    readonly opacity: number;
    readonly specular: number;
    readonly emissive: number;
  };
  readonly glitter: {
    readonly opacity: number;
    readonly specular: number;
    readonly emissive: number;
  };
}

export interface CleanRoomVolumeProfile {
  readonly slug: string;
  readonly cloth: string;
  readonly ink: string;
  readonly paper: string;
  readonly headband: readonly [string, string];
  readonly artworkUrl: string;
  readonly caption: string;
  /** Full book thickness in centimetres (the generated mesh is 3.374 thick). */
  readonly thickness: number;
  readonly spineNote?: string;
  readonly material: CleanRoomMaterialProfile;
}

/**
 * Per-volume inputs for the shared book mesh. Geometry is deliberately absent:
 * every volume uses the same generated mesh and changes thickness through a
 * uniform that scales it across its thickness.
 */
export const demoVolumeProfiles = [
  {
    slug: "surfaces",
    cloth: "#ababab",
    ink: "#000000",
    paper: "#f1f1f1",
    headband: ["#1d1d1d", "#bababa"],
    artworkUrl: surfacesArtwork,
    caption: "Compare cloth, paper, foil, gloss, and bump responses on the shared book mesh.",
    thickness: 3.4,
    material: {
      shininess: 3,
      specular: "#ffffff",
      reflectiveness: 0.6,
      diffuseBaseColor: "#101010",
      baseBump: "none",
      bump: { base: -0.02, custom: -0.04 },
      foil: {
        colors: ["#7b7b7b", "#c2c2c2"],
        detail: 2,
        opacity: 0,
        specular: 0.2,
        emissive: 0
      },
      gloss: { opacity: 1, specular: 0.1, emissive: 0 },
      glitter: { opacity: 0.3, specular: 0.2, emissive: 0 }
    }
  },
  {
    slug: "geometry",
    cloth: "#d0d0d0",
    ink: "#000000",
    paper: "#f8f8f8",
    headband: ["#3f3f3f", "#d0d0d0"],
    artworkUrl: geometryArtwork,
    caption: "Reuse one authored topology while varying thickness, covers, joints, and page blocks.",
    thickness: 3.18,
    spineNote: "Topology",
    material: {
      shininess: 2,
      specular: "#e4e4e4",
      reflectiveness: 0.1,
      diffuseBaseColor: "#3f3f3f",
      baseBump: "buckram",
      bump: { base: 0.04, custom: 0.25 },
      foil: {
        colors: ["#0a0a0a", "#141414"],
        detail: 2.6,
        opacity: 0,
        specular: 0,
        emissive: 0
      },
      gloss: { opacity: 1, specular: 0.1, emissive: 0 },
      glitter: { opacity: 0, specular: 0, emissive: 0 }
    }
  },
  {
    slug: "interaction",
    cloth: "#323232",
    ink: "#ffffff",
    paper: "#f8f8f8",
    headband: ["#b9b9b9", "#323232"],
    artworkUrl: interactionArtwork,
    caption: "Exercise picking, holding, dragging, focus, and the transition from shelf to volume.",
    thickness: 3.4,
    spineNote: "Input model",
    material: {
      shininess: 1.2,
      specular: "#eaeaea",
      reflectiveness: 0.8,
      diffuseBaseColor: "#323232",
      baseBump: "none",
      bump: { base: 0.015, custom: 0.14 },
      foil: {
        colors: ["#b9b9b9", "#efefef"],
        detail: 1.5,
        opacity: -1,
        specular: 0.3,
        emissive: -1
      },
      gloss: { opacity: 1, specular: 0.1, emissive: 0 },
      glitter: { opacity: 0, specular: 0, emissive: 0 }
    }
  },
  {
    slug: "routing",
    cloth: "#3a3a3a",
    ink: "#ffffff",
    paper: "#f8f8f8",
    headband: ["#e0e0e0", "#3a3a3a"],
    artworkUrl: routingArtwork,
    caption: "Keep one scene alive while routes change, volumes scroll, and navigation returns to the shelf.",
    thickness: 2.85,
    spineNote: "History",
    material: {
      shininess: 1,
      specular: "#e2e2e2",
      reflectiveness: 0.6,
      diffuseBaseColor: "#3a3a3a",
      baseBump: "paper",
      bump: { base: 0.015, custom: 0.12 },
      foil: {
        colors: ["#e0e0e0", "#a9a9a9"],
        detail: 1,
        opacity: 0.8,
        specular: -0.45,
        emissive: 0.45
      },
      gloss: { opacity: 1, specular: 0.1, emissive: 0 },
      glitter: { opacity: 0, specular: 0, emissive: 0 }
    }
  },
  {
    slug: "integration",
    cloth: "#7d7d7d",
    ink: "#000000",
    paper: "#f8f8f8",
    headband: ["#313131", "#7d7d7d"],
    artworkUrl: integrationArtwork,
    caption: "Mount the renderer over ordinary links and retain an accessible catalogue without WebGL.",
    thickness: 3.32,
    spineNote: "Fallback",
    material: {
      shininess: 1,
      specular: "#e2e2e2",
      reflectiveness: 0.6,
      diffuseBaseColor: "#313131",
      baseBump: "cardboard",
      bump: { base: 0.04, custom: -0.04 },
      foil: {
        colors: ["#0a0a0a", "#141414"],
        detail: 0.8,
        opacity: 0,
        specular: 0,
        emissive: 0
      },
      gloss: { opacity: 1, specular: 0.1, emissive: 0 },
      glitter: { opacity: 0, specular: 0, emissive: 0 }
    }
  }
] as const satisfies readonly CleanRoomVolumeProfile[];
