# press

ZI3T Volume Catalogue: a persistent 3D book catalogue in the browser,
inspired by Stripe Press. The scene is C11 + raylib compiled to WebGL 2
(`src/press/c/`). The page half (routing, scroll, hit targets) is TypeScript
in `src/press/runtime/`.

Archived. Live at https://zi3t.github.io/press/.

- `npm install && npm run dev` runs it locally
- `npm test` runs the shelf interaction tests
- `npm run build:press` rebuilds `public/viewer/` (needs Emscripten and raylib built for WebGL 2)

Not affiliated with or endorsed by Stripe. See `src/press/README.md` for
provenance and the values taken from the reference site.
