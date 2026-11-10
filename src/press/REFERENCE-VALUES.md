# Press: values derived from the live Stripe Press site

An inventory for legal review. It lists every value and behaviour in the
current Press source that was taken from, measured on, or fitted to
[press.stripe.com](https://press.stripe.com/), or to earlier builds of this
project that used copied Stripe material. It does not argue whether any item
is protectable. That is the reviewer's call. It records where each item lives
and how it was obtained.

Paths are relative to `src/press/` unless they start with `scripts/`,
or `src/pages/`.

## How to read the classes

| Class | Meaning |
| --- | --- |
| **Measurement** | A number read off the running reference: a camera or object transform, a uniform value captured at the GPU (with the since-removed `tests/capture-glframe.mjs`), a screen-space distance, a scroll ratio. No reference code was copied to produce it, but the number is the reference's. |
| **Behaviour** | An observable interaction rule (what happens on scroll, Back, return), reimplemented here. The rule is the reference's; the code is this project's. |
| **Code-derived** | A constant or function shape read from Stripe's shipped JavaScript bundle (the comments name the bundle function). Higher risk than a measurement, because the source of the number is the reference's code. |
| **Fitted to copied material** | A value chosen so this project's original replacement matches how an earlier build looked, when that earlier build used material later found to be copied (see [Assets removed from the tree](#assets-removed-from-the-tree)). The replacement is original; the target it was tuned to was not. |

## 1. Camera and shelf layout: `CLEAN_ROOM_REFERENCE`

Defined twice with the same values: `runtime/clean-room/motion.ts`
(`CLEAN_ROOM_REFERENCE`) and `c/press.h` (`REF_*`). The `motion.ts` doc comment
says: *"Values read from the current Stripe Press camera and shared book scene."*

| Value | Meaning | Class |
| --- | --- | --- |
| `canvasMaxWidth` 2000, `canvasReferenceHeight` 1018, `canvasOverscan` 1.1, `positionReferenceWidth` 1792 | Canvas sizing and the width the active-book position is scaled against | Measurement |
| `cameraBaseZ` 100, `cameraY` 6.5, `cameraPitch` −0.06 | Camera placement | Measurement |
| `catalogueCameraScroll` 0.0222 | World units the camera descends per scrolled pixel (at the reference height) | Measurement |
| `modelScale` 1, `shelfRootZ` −3, `shelfCoverOffsetX` 11 | Shelf book transforms | Measurement |
| `shelfHoverZ` 1.5 | The *observed equilibrium* of the reference's hover pull (toward z = 6) against its shelf pull (toward z = −3); see the comment in `motion.ts` | Measurement (of an interaction between two reference behaviours) |
| `shelfRootRotationX` −π/2, `shelfRootRotationZ` π/2, `coverBaseRotationY` −π/2 | Shelf orientation | Measurement |
| `activeX` −13, `activeY` −4, `activeZ` −56, `inactiveZ` −50 | Open-book position | Measurement |
| `activeRotationX` −0.5, `activeRotationY` 0.35, `activeRotationZ` 0.15 | Open-book pose | Measurement |
| `compactActiveX` 0, `compactActiveY` 3, `compactActiveZ` −90, `narrowActivePitchOffset` 0.16 | Phone-width open-book pose | Measurement |
| Shelf gap −6 (−7 narrow) in `c/press_scene.c` `stage_reference_list_return` and the shelf layout | Vertical spacing between shelf books | Measurement |

## 2. Motion: `CLEAN_ROOM_MOTION`

`runtime/clean-room/motion.ts` and `c/press.h` (`MOTION_*`).

| Value | Meaning | Class |
| --- | --- | --- |
| `entryInitialY` 3, `entryInitialGap` 3, `entryInitialZ` −50, `entryInitialDepthArc` 150, `entrySettleEpsilon` 0.012 | First-load entry of the shelf | Measurement |
| `flightEaseStep` 0.006, `flightEaseCeiling` 0.15 | The books' shared transition-speed recurrence (each frame the approach rate grows by a step up to a ceiling). AGENTS.md calls it "the books' shared transition-speed recurrence". | Code-derived (the recurrence's form and constants) |
| `releaseIsolation` 360, `releasePresentation` 110, `releaseBackdrop` 430, `releaseClassDelay` 780 (ms) | Timings when a held book is released | Measurement |
| `volumeFollowRate` 0.00015, `volumeDragRate` 0.003, `volumeScrollTurn` 0.0008, `volumeTwirlLimit` 0.3, `volumeTwirlDecay` 0.95 | Open-book drag and scroll rotation | Measurement |
| `scrollVelocityPerPixel` 0.003, `scrollVelocityDecay` 0.4, `scrollVelocityLimit` 1 | Shelf tilt from scroll speed | Measurement |
| `stackEvictionViewports` 1.12, `spineZEase` 0.1, `dragThreshold` 4, `heldLiftPixels` 0 | Shelf stacking and hold | Measurement |
| `terminalScrollViewports` 3.36 | Length of the terminal (film → signal → closing) track in viewports | Measurement |
| `idlePauseAfter` 1200 (ms) | Idle render pause; `scene.ts` notes that the reference keeps this path only on desktop Chromium | Behaviour |
| Damping speeds in `c/press_scene.c` (13 for shelf transforms, 7.5 for cover turn-back, 5.2 for the rake light) | Per-frame smoothing | Measurement |

## 3. Lighting rig

`c/press_render.c` `set_book_lights` and `c/press_scene.c` `lights_update`
(previously `lighting.ts`, `createCleanRoomLightRig`).

| Value | Meaning | Class |
| --- | --- | --- |
| Ambient 0.52; key directional 0.6 from (4, 9.5, 4.5); back directional 0.5 from (−32, 12, −16) | Light intensities and directions | Measurement (uniforms captured at the GPU) |
| `LEGACY_LIGHT_SCALE` = π on every light | Matches three.js r151's legacy-lights premultiply, as observed in the reference's uniforms | Measurement |
| Back-light rest colour `#1a1a1a`, and its blend toward the active book's cloth colour | Back light tint | Measurement / Behaviour |
| Rake spot at (24, cameraY, 1), aimed at (−6 → −14.3, cameraY − 6.5, −6.5 → −61) between shelf and open book; cone 0.36 rad, no penumbra; colour `#e4e4e4` × 0.75 on the shelf, × 0.05 open | Raking spot light | Measurement |

The Blinn-Phong functions in `c/shaders/book.fs` are ported from three.js (MIT).
They are not reference-derived.

## 4. Film poster (terminal media)

`c/press_scene.c` `advance_poster`, `stage_reference_list_return`;
`c/press_render.c` poster mesh. AGENTS.md requires the film to follow the
reference's staging.

| Value | Meaning | Class |
| --- | --- | --- |
| `PlaneGeometry(22, 30, 99, 99)` | Film sheet size and subdivision | Code-derived |
| Deformation = `(filmTop + viewportHeight/2 − scrollY) × 0.0017`, clamped to [0.08, 1.4] | How folded the poster is at a given scroll | Code-derived. The comment says *"Exact updateFilmPosters scalar from Stripe's current Canvas bundle."* |
| Poster z = −18 (−60 on small screens) − (1 − deformation) × 50; z-rotation = deformation / 25; pointer tilt limit 0.3 (`FILM_TILT_LIMIT`) | Poster placement | Code-derived |
| −33 film gap (`FILM_RESET_GAP`) and −33 × hold isolation offset | Where the film is staged when returning to the list | Code-derived (`transformResetProductList`) |
| The fold shape itself | Five-panel accordion, inextensible, hanging from the top edge (`poster_set_deformation`) | **Original** as of this branch: it replaces a port of `updateFilmPosters`/`createFilmDeltas` |
| `POSTER_DRAW_UP` 0.63, `POSTER_MIN_DROP` 0.12 | Folded height as a fraction of flat height | **Fitted to copied material**: chosen so the folded height matches the previous build, whose fold was the `updateFilmPosters` port |

## 5. Return to the catalogue

`c/press_scene.c` `stage_reference_list_return`, `runtime/clean-room/routing.ts`.

| Item | Class |
| --- | --- |
| On return, each book is staged at `activeY + (activeIndex − i) × 100` plus the shelf gap, then the shared recurrence brings them home, so the last active book is centred. AGENTS.md requires this rather than restoring the old scroll offset. Ported from `transformResetProductList`. | Behaviour; the ×100 staging offset is code-derived |
| One scene, renderer and set of book meshes survive every route change | Behaviour |

## 6. Scroll and terminal track

`runtime/clean-room/catalogue-scroll.ts`, `runtime/clean-room/scene.ts`.

| Value | Meaning | Class |
| --- | --- | --- |
| Scroll step = viewport height × 0.213 (× 0.225 on compact layouts) (`cleanRoomCatalogueScrollStep`) | Scroll distance per shelf book | Measurement |
| Film starts immediately after the fifth book, then the signal scene and the light closing surface follow on one 1 px : 1 px vertical track | Terminal sequence | Behaviour |
| Terminal opacity reaches 1 after 0.06 viewports; closing surface starts at 2.42 viewports; rail and help fade over the last 0.34 viewports before it | Terminal timing | Measurement |
| Masthead and controls stay in dark-surface colours until the light surface reaches the header | Behaviour |

## 7. Routing and history

`runtime/clean-room/routing.ts`, `src/pages/press/[...slug].astro`.

| Item | Class |
| --- | --- |
| Leaving the list pushes one history entry; moving between volumes replaces it, so Back from any volume returns to the list in one step | Behaviour (comment: *"Stripe adds one history entry when leaving the product list, then replaces that entry…"*) |
| Each volume has its own deep-link path (`/press/<slug>/`) served by the same shell | Behaviour (the URL scheme is the reference's pattern; the slugs are this project's) |

## 8. Book mesh, atlas layout and material interface

The mesh and its maps are now generated by `c/press_book.c`, and the shader
was rewritten. Their **dimensions and layout** still follow the removed
`book.obj` (see below), which was of unrecorded origin, most likely Stripe's.

| Value | Where | Class |
| --- | --- | --- |
| Book size 23.68 × 3.374 × 15.9 cm (`HALF_HEIGHT` 11.84, `BOOK_MODEL_THICKNESS` 3.374, fore edge −7.95, spine ≈ 7.96) | `c/press_book.c`, `c/shaders/book.vs` | **Fitted to copied material** (the removed OBJ's bounding box, kept so hit targets and shelf spacing are unchanged) |
| Construction: the case as one sheet bent along a single cross-section and rolled at every edge, the text block fitted inside | `c/press_book.c` | Original code. The construction principle was learned by studying the removed OBJ's topology (one continuous case shell, separate endbands, a three-face page block); it is also simply how a cased book is made |
| Board 2.4 mm (`CASE_R` 0.12), 8 mm French joint 1.3 mm deep, spine shoulder radius 0.24, squares 0.30 (cm) | `c/press_book.c` | Original: ordinary case-binding proportions, judged by eye against the previous build; not taken from the OBJ's coordinates |
| Thickness change applied as a uniform on one shared mesh | `book.vs`, `profiles.ts` `thickness` | Behaviour (the reference widens one mesh per book) |
| Atlas 1920 × 1600: cloth wrap from row 320; spine columns 874–1046; foil palette tile 14 % × 19 % in the top-left; endband swatch 421–616 × 197–249 | `runtime/clean-room/textures.ts`, `c/press_book.c` | **Fitted to copied material** (the removed OBJ's UV islands) |
| Wrap print scale about 51–53 atlas px/cm (`WRAP_TOP` 356 / `WRAP_BOTTOM` 1568, front 1070–1884, back 36–850) | `c/press_book.c` | **Fitted to copied material**: tuned to match the previous build's type size |
| Per-volume material parameters: shininess, specular, reflectiveness, `diffuseBaseColor`, base/custom bump scale, foil (detail, opacity, specular, emissive, two colours), gloss and glitter (opacity, specular, emissive) | `runtime/clean-room/profiles.ts`, uniforms in `book.fs` | The **parameter set and names** mirror the reference material's uniform interface (captured with the since-removed `capture-glframe.mjs`). The values are per demo volume; whether any were copied from captured reference uniforms is **not recorded** |
| The book shader is fully opaque; transitions move or hide meshes rather than fade them | `c/press_scene.c` `set_book_opacity` | Behaviour |

## 9. CSS calibrated against reference screenshots

These stylesheets describe themselves as matched to the reference:

- `styles/home-redesign.css`: *"Screenshot-led replication of the Stripe Press opening canvas."*
- `styles/press-fidelity.css`: *"Final screenshot-matching layer for the Stripe Press catalog treatment."*
- `styles/press-calibration.css` and `styles/volume-sections.css`: positions, the rest ground `#000000`, the themed menu cluster (rail, return, About marker follow the active volume's ink).

Class: measurement and behaviour (layout positions, sizes, timings and the
menu's colour rule), with the site's own monochrome tokens, type and copy.

## 10. Tools that read the reference (removed)

Until 2026-10-09 the repository carried three tools used to take the readings
above; they were deleted, with the rest of the material they produced, before
publication:

| File (removed) | What it did |
| --- | --- |
| `tests/capture-reference.mjs` | Drove press.stripe.com and downloaded every file it fetched into a hash-pinned manifest. |
| `tests/capture-glframe.mjs` | Recorded WebGL calls on a page, including shader sources and uniform values. |
| `tests/cdp.mjs` | Chrome DevTools Protocol helper for the two above. |

They are listed here because they explain how the measurements in this
inventory were made.

## Assets removed from the tree

These are no longer in the working tree, and on 2026-10-09 they were
**purged from git history** (`git filter-repo`, every branch and tag) along
with every other copy of them. GitHub's read-only pull-request refs and PR
pages still show the old commits until GitHub Support clears them.

| Path (last location) | Finding |
| --- | --- |
| `assets/textures/shared-bump-buckram.jpg`, `shared-bump-cardboard.jpg`, `shared-bump-paper.jpg`, `shared-glitter.png` | **Byte-identical** (SHA-256) to `shared_bump_buckram.jpg`, `shared_bump_cardboard.jpg`, `shared_bump_paper.jpg` and `shared_glitter.png` on Stripe's Contentful CDN (`images.ctfassets.net/fzn2n1nzq965/…`), as listed in the capture manifest that was once committed at `docs/reference/manifest-20260807.json` (since purged). These are copies of Stripe's files. |
| `assets/textures/shared-diffuse-overlay.jpg`, `shared-bump-none.jpg` | Same names as Stripe's `shared_diffuse_overlay` and `shared_bump_none`; no byte match against the WebP variants in the manifest, so origin is unconfirmed. |
| `assets/book.obj` (Blender 3.6 export) | Origin not recorded. It is not in the manifest (the reference ships its model inside a JS chunk), so a hash check is impossible. The atlas layout it defines matches the reference's texture atlases. |
| `runtime/clean-room/book.vert.glsl`, `book.frag.glsl` (later `c/shaders/book.*` before this branch) | Origin not recorded; uniform names and structure match the reference material interface. |
| `runtime/clean-room/foldout-poster.ts` and the `create_film_deltas` port | A port of the reference's `updateFilmPosters` deltas. |
| `public/press-assets/chunks/*.js` (built bundles, committed until 2026-09-20) | Embed the above shaders and data. |
| `docs/reference/*.json` (committed 2026-08-07 to 2026-08-09) | Captured readings from press.stripe.com: uniform values, draw counts, asset URLs and SHA-256s, screenshot measurements. No shader source or asset bytes. |
