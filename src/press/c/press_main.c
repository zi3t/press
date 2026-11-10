// Entry point for the Press module. main() only opens the GL context on the
// page's canvas; scene.ts drives everything else through the press_* exports
// from its own animation frame.
#include "raylib.h"

#include "press.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#define EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT
#endif

// RGBA8 pixels the page placed in the heap, top row first.
EXPORT void press_upload_texture(int id, const unsigned char *rgba, int width, int height) {
  render_upload_texture(id, rgba, width, height);
}

int main(void) {
  SetTraceLogLevel(LOG_WARNING);
  SetConfigFlags(FLAG_MSAA_4X_HINT);
  InitWindow(1, 1, "press");
  return 0;
}
