# Shared book textures

This directory holds no texture files. The maps every book shares are
generated in code when the Press module starts, by `book_generate_maps` in
`src/press/c/press_book.c`, and uploaded straight to the GPU:

| Map | Size | How it is made |
| --- | --- | --- |
| Shading overlay | 1920 × 1600 | Laid out for the generated mesh's UVs. 128 is neutral; the cloth sits slightly darker, with a faint value-noise mottle (the case's own shape shades its joints and rolled edges). Page edges get one fine band per sheet, darker where the boards overhang them; a small patch of shaded cloth stands for the turn-in inside the boards. |
| Buckram relief | 1920 × 1600 | A plain weave: warp and weft threads alternating over and under on a 4 px period, with uneven thread thickness, loose-fibre grain and per-cell jitter. |
| Paper relief | 1920 × 1600 | Four octaves of value noise. |
| Cardboard relief | 1920 × 1600 | Fine grain plus sparse short fibres. |
| No relief | 64 × 64 | Flat neutral grey. |
| Glitter | 100 × 100 | Dim flakes with a few bright ones, from a hash. |

Every map comes from a fixed integer hash, so it is identical on every run.
Their provenance is that source file. The per-volume cover atlases (print,
pressed relief, foil and gloss masks) are painted by the page in
`src/press/runtime/clean-room/textures.ts` from this project's SVG artwork and
type.

## History

The `shared-*` files that used to live here have been removed. Four of them
(`shared-bump-buckram.jpg`, `shared-bump-cardboard.jpg`,
`shared-bump-paper.jpg`, `shared-glitter.png`) were byte-identical to files of
the same names served by Stripe Press; see `src/press/REFERENCE-VALUES.md`.
They were purged from the repository's git history on 2026-10-09. Earlier still, this directory held
CC0 scans (Poly Haven `Book Pattern`, ambientCG `Paper 001`). That record does
not apply to any later file.
