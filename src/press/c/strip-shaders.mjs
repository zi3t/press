// Copies shaders/ to build/shaders/ without comments, indentation or blank
// lines, so the GLSL embedded in press.wasm carries no notes or provenance.
// Preprocessor lines keep their own line; GLSL has no string literals, so
// stripping // and /* */ cannot cut through one.
import { mkdirSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";

const [src = "shaders", out = "build/shaders"] = process.argv.slice(2);
mkdirSync(out, { recursive: true });

for (const name of readdirSync(src)) {
  const text = readFileSync(join(src, name), "utf8")
    .replace(/\/\*[\s\S]*?\*\//g, " ")
    .replace(/\/\/.*$/gm, "");
  const lines = text
    .split("\n")
    .map((line) => line.replace(/\s+/g, " ").trim())
    .filter(Boolean);
  writeFileSync(join(out, name), lines.join("\n") + "\n");
}
