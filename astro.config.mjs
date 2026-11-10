// @ts-check
import { defineConfig } from "astro/config";

// Served by GitHub Pages as a project site at zi3t.github.io/press/.
// Every route is a directory: /press/ and one copy of the shell per /press/<slug>/.
export default defineConfig({
  site: "https://zi3t.github.io",
  base: "/press",
  output: "static",
  prefetch: false,
  trailingSlash: "always",
  build: { format: "directory" },
  compressHTML: false,
  vite: {
    // Keep SVGs and textures as real files with stable URLs.
    build: { assetsInlineLimit: 0, chunkSizeWarningLimit: 700 }
  }
});
