# The Press scene as a raylib/Emscripten module, committed to
# public/viewer/ because CI has no Emscripten (npm run build:press).
# Needs Homebrew emscripten and raylib built for WebGL 2 (RAYLIB_WEB_ES3).
RAYLIB_WEB_ES3 ?= $(HOME)/Developer/Personal/raylib/src-es3
OUT := ../../../public/viewer
SOURCES := press_main.c press_scene.c press_render.c press_book.c press_signal.c
HEADERS := press.h press_math.h
SHADERS := $(wildcard shaders/*)

.PHONY: site clean

site: $(OUT)/press.mjs

# Shaders are embedded from build/shaders/, stripped of comments (strip-shaders.mjs).
build/shaders: $(SHADERS) strip-shaders.mjs
	rm -rf $@
	node strip-shaders.mjs shaders $@

# -O3 inlines more of the C, so fewer function boundaries survive. Only the
# _press_* exports and FS/HEAP* keep readable names, as scene.ts needs them.
$(OUT)/press.mjs: $(SOURCES) $(HEADERS) build/shaders $(RAYLIB_WEB_ES3)/libraylib.web.a
	@mkdir -p $(OUT)
	emcc -std=c11 -O3 -Wall -Wextra -DPLATFORM_WEB -I$(RAYLIB_WEB_ES3) $(SOURCES) $(RAYLIB_WEB_ES3)/libraylib.web.a \
		-sUSE_GLFW=3 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFULL_ES3 -sALLOW_MEMORY_GROWTH \
		-sMODULARIZE -sEXPORT_ES6 -sEXPORT_NAME=createPress -sENVIRONMENT=web -sFORCE_FILESYSTEM \
		-sEXPORTED_FUNCTIONS=_main,_malloc,_free -sEXPORTED_RUNTIME_METHODS=FS,HEAPU8,HEAPF64 \
		--embed-file build/shaders@shaders -o $@

clean:
	rm -rf build $(OUT)/press.mjs $(OUT)/press.wasm
