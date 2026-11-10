# Stripe Press–inspired WebGL book-catalogue

## Current status

Archived. Published under the MIT licence (see `LICENSE`). The open items in
[Rights, provenance, and release gate](#rights-provenance-and-release-gate)
are unresolved; the MIT licence covers this project's own work only and grants
nothing in Stripe's marks, designs or code.

## What it is

An independent raylib (C, WebGL 2) study of a Stripe Press–style persistent 3D book
catalogue: one scene, renderer and set of meshes that survive internal route
changes, with ordinary links in `src/pages/[...slug].astro` as the
semantic, no-WebGL fallback. The 3D scene is in `c/` (built into
`public/viewer/`), its page half in `runtime/clean-room/`, demo content in
`content/volumes.ts`, and styles in `styles/` (paths relative to this
directory); Astro builds one copy of that static shell per deep link.

## SEO and naming

The source-identifying name remains **ZI3T Volume Catalogue**. “Stripe
Press–inspired” appears only in descriptive copy so readers can understand the
reference accurately.

Do not rename the package, repository, domain, or social account to include
“Stripe” or “Stripe Press”; do not use Stripe logos; do not present the Stripe
reference more prominently than the ZI3T project name; and do not imply an
official relationship. These boundaries follow
[Stripe's Mark Usage Terms](https://stripe.com/legal/marks).

## Reference credit and non-affiliation

[Stripe Press](https://press.stripe.com/) is a Stripe publishing project. Its
current site footer states `© 2026 Stripe, LLC`.

Public creator credits consulted for this project:

- [Yuin Chien](https://yuinchien.com/p/stripe-press) says she led the 2021 3D web
  design in collaboration with Nick Jones and Philipp Antoni.
- [Nick Jones](https://www.narrowdesign.com/) describes bringing the Stripe
  Press books to life in 3D and lists Yuin Chien, Devin Jacoviello, Philipp
  Antoni, Kate Lee, Tamara Winter, and Patrick Collison as collaborators. He
  also credits the referenced Stripe Press book covers to Tyler Thompson.

Those are public self-credits, not an official or exhaustive production credit
list. Please open an issue if a verified credit is missing or inaccurate.

Stripe, Stripe Press, their marks, original site, books, cover designs, and
editorial material remain the property of their respective owners. Attribution
does not grant a licence and does not cure unauthorized copying.

## Rights, provenance, and release gate

Recorded project-owned or permitted material:

- neutral demo names, text, SVG fixtures, and colour palettes are ZI3T work;
- the poster artwork is ZI3T work, documented in `assets/media/README.md`;
- the book mesh (boards, rounded spine, grooved hinges, page block, endbands)
  and its shared maps (shading overlay, buckram/paper/cardboard relief,
  glitter tile) are generated at start-up by `c/press_book.c`; no mesh or
  texture file is loaded for them (see `assets/textures/README.md`);
- `c/shaders/book.vs` and `c/shaders/book.fs` were rewritten for this project
  (print × overlay, a relief normal after M. Mikkelsen, "Bump Mapping
  Unparametrized Surfaces on the GPU", 2010, metal-leaf foil reflecting the
  atlas's foil tile, lacquer and glitter); only the Blinn-Phong lighting
  functions are ported from three.js shader chunks, under its MIT licence;
- the film poster's fold is an original model (`poster_set_deformation` in
  `c/press_render.c`), replacing a port of the reference's poster function;
- the signal scene's lighting and built-in geometries are ported from three.js
  (MIT); raylib (zlib licence) and the Emscripten runtime (MIT) are compiled
  into `public/viewer/`; Departure Mono is used under the SIL Open Font
  License (`assets/fonts/DepartureMono-LICENSE.txt`); and
- the historical Poly Haven and ambientCG surface scans were CC0, but those
  files were later removed and do **not** establish the provenance of any later
  file.

| Scope | Status |
| --- | --- |
| `assets/book.obj` | **Removed** from the tree; replaced by the generated mesh in `c/press_book.c`. Its origin was never recorded. Purged from git history (2026-10-09). |
| `assets/textures/shared-*` | **Removed**; replaced by maps generated in `c/press_book.c`. Four of the removed files are byte-identical to Stripe's CDN files of the same names (see `REFERENCE-VALUES.md`). Purged from git history (2026-10-09). |
| `c/shaders/book.vs`, `c/shaders/book.fs` | **Rewritten** on this branch; the previous material code (origin not recorded) was purged from git history (2026-10-09). |
| Exact live-reference values in `runtime/clean-room/motion.ts`, `c/press.h`, `c/press_scene.c`, `c/press_render.c`, `runtime/clean-room/catalogue-scroll.ts`, `runtime/clean-room/routing.ts` and the calibration stylesheets | **Unresolved.** Inventoried in [`REFERENCE-VALUES.md`](REFERENCE-VALUES.md), split into measurements, behaviour, values read from the reference's code, and values fitted to the earlier copied material. A qualified review must distinguish uncopyrightable observations and interfaces from protected expression or copied implementation. |
| Mesh dimensions and atlas layout | **Unresolved.** The generated mesh is original code, but its size and UV layout were fitted to the removed `book.obj` so the existing atlas painter (`runtime/clean-room/textures.ts`) still applies. |
| Git history | **Purged on 2026-10-09** with `git filter-repo`: the removed mesh, textures and shaders, the poster port, old builds and bundles that embed them, and the captured reference readings are gone from every branch and tag. **Still open:** GitHub keeps read-only pull-request refs (`refs/pull/*`) and PR pages that show the old commits until GitHub Support clears them, or the history moves to a fresh repository. |

Before a wider release, complete one of these paths for every unresolved item:

1. record verifiable original authorship and source history;
2. replace it with independently created material and preserve that provenance;
3. document a compatible third-party licence; or
4. obtain written permission from the relevant rights holder.

Then obtain a qualified trademark/copyright review for the intended countries
and use. Stripe directs mark-usage questions to `trademarks@stripe.com`.

Until that gate is complete, do not publish a package release, sell or
merchandise the work, claim Stripe approval, or label the implementation
“legally cleared.”
