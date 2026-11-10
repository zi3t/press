export interface ProductDefinition {
  readonly routeUrl: string;
  readonly slug: string;
  readonly title: string;
  readonly byline: string;
  readonly description: string;
}

export interface VolumeDefinition extends ProductDefinition {
  readonly spine: {
    readonly eyebrow: string;
    readonly serial: string;
    readonly ariaLabel: string;
  };
}

/**
 * Self-contained fixtures for the package demo. They describe catalogue
 * capabilities, not pages or content owned by the host application.
 */
export const demoVolumes = [
  {
    routeUrl: "/press/surfaces/",
    slug: "surfaces",
    title: "Surface studies",
    byline: "Materials / light / print",
    description:
      "Compare cloth, paper, foil, gloss, and bump responses on the shared book mesh.",
    spine: {
      eyebrow: "Materials",
      serial: "01",
      ariaLabel: "Open the surface studies demo"
    }
  },
  {
    routeUrl: "/press/geometry/",
    slug: "geometry",
    title: "Shared geometry",
    byline: "Mesh / joints / thickness",
    description:
      "Reuse one authored topology while varying thickness, covers, joints, and page blocks.",
    spine: {
      eyebrow: "Geometry",
      serial: "02",
      ariaLabel: "Open the shared geometry demo"
    }
  },
  {
    routeUrl: "/press/interaction/",
    slug: "interaction",
    title: "Shelf interaction",
    byline: "Pointer / keyboard / touch",
    description:
      "Exercise picking, holding, dragging, focus, and the transition from shelf to volume.",
    spine: {
      eyebrow: "Interaction",
      serial: "03",
      ariaLabel: "Open the shelf interaction demo"
    }
  },
  {
    routeUrl: "/press/routing/",
    slug: "routing",
    title: "Route lifecycle",
    byline: "History / deep links / return",
    description:
      "Keep one scene alive while routes change, volumes scroll, and navigation returns to the shelf.",
    spine: {
      eyebrow: "Routing",
      serial: "04",
      ariaLabel: "Open the route lifecycle demo"
    }
  },
  {
    routeUrl: "/press/integration/",
    slug: "integration",
    title: "Host integration",
    byline: "Semantic HTML / WebGL enhancement",
    description:
      "Mount the renderer over ordinary links and retain an accessible catalogue when WebGL is unavailable.",
    spine: {
      eyebrow: "Integration",
      serial: "05",
      ariaLabel: "Open the host integration demo"
    }
  }
] as const satisfies readonly VolumeDefinition[];

export const demoFilm = {
  kind: "film",
  routeUrl: "/press/film/",
  slug: "film",
  title: "Moving pictures",
  byline: "A study in paper and motion",
  description: "An illustrated poster unfolds into a study of light, paper, and movement."
} as const satisfies ProductDefinition & { readonly kind: "film" };

export const demoSignal = {
  kind: "signal",
  routeUrl: "/press/signal/",
  slug: "signal",
  title: "Signal in motion",
  byline: "A study in waves, layers, and response",
  description: "A medallion turns, opens, and sends concentric waves across an abstract landscape."
} as const satisfies ProductDefinition & { readonly kind: "signal" };

export const demoProducts = [
  ...demoVolumes.map((volume) => ({ ...volume, kind: "book" as const })),
  demoFilm,
  demoSignal
] as const;

export const withTrailingSlash = (pathname: string): string => (
  pathname.endsWith("/") ? pathname : `${pathname}/`
);
