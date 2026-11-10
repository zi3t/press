// Drawing for the Press scene: the books (mesh and shared maps from
// press_book.c, material in book.fs/book.vs, the four-light rig), the folded
// film poster, and the GL state they draw with.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include "press.h"

double book_bounds_min[3], book_bounds_max[3];

static Texture2D textures[TEX_COUNT];
static Mesh book_mesh;
static Shader book_shader, basic_shader;
static Material book_material, basic_material;
static int framebuffer_width, framebuffer_height;

// raylib's Matrix holds the same column-major element mN as three.js's
// elements[N], but declares its fields m0, m4, m8, m12, m1, ... so set them
// by name rather than by memory order.
Matrix to_rl(M4 m) {
  const double *e = m.e;
  return (Matrix){(float)e[0], (float)e[4], (float)e[8], (float)e[12],
                  (float)e[1], (float)e[5], (float)e[9], (float)e[13],
                  (float)e[2], (float)e[6], (float)e[10], (float)e[14],
                  (float)e[3], (float)e[7], (float)e[11], (float)e[15]};
}

// The shaders are GLSL 3.30; the web runs the same source as GLSL ES 3.00.
static char *shader_source(const char *path) {
  char *text = LoadFileText(path);
  if (!text) return NULL;
#if defined(PLATFORM_WEB)
  const char *es = "#version 300 es\nprecision highp float;";
  const char *rest = strstr(text, "#version 330") + strlen("#version 330");
  char *out = MemAlloc((unsigned)(strlen(es) + strlen(rest) + 1));
  strcpy(out, es);
  strcat(out, rest);
  UnloadFileText(text);
  return out;
#else
  return text;
#endif
}

static Shader load_shader(const char *vs_path, const char *fs_path) {
  char *vs = shader_source(vs_path), *fs = shader_source(fs_path);
  Shader shader = LoadShaderFromMemory(vs, fs);
  MemFree(vs);
  MemFree(fs);
  return shader;
}

static int loc(Shader s, const char *name) { return GetShaderLocation(s, name); }
static void set_f(Shader s, const char *name, double v) { float f = (float)v; SetShaderValue(s, loc(s, name), &f, SHADER_UNIFORM_FLOAT); }
static void set_v3(Shader s, const char *name, V3 v) { float f[3] = {(float)v.x, (float)v.y, (float)v.z}; SetShaderValue(s, loc(s, name), f, SHADER_UNIFORM_VEC3); }
static void set_rgb(Shader s, const char *name, RGB c, double k) { set_v3(s, name, v3(c.r * k, c.g * k, c.b * k)); }

// ── foldout poster mesh ────────────────────────────────────────────────────

#define GRID 100
#define FILM_WIDTH 22.0
#define FILM_HEIGHT 30.0
static Mesh poster_mesh;
static float poster_base[GRID * GRID * 3];

// PlaneGeometry(22, 30, 99, 99): rows from the top, columns from the left.
static void create_poster_mesh(void) {
  Mesh m = {0};
  m.vertexCount = GRID * GRID;
  m.triangleCount = (GRID - 1) * (GRID - 1) * 2;
  m.vertices = MemAlloc(sizeof(float) * 3 * m.vertexCount);
  m.texcoords = MemAlloc(sizeof(float) * 2 * m.vertexCount);
  m.indices = MemAlloc(sizeof(unsigned short) * 3 * m.triangleCount);
  for (int iy = 0; iy < GRID; iy += 1)
    for (int ix = 0; ix < GRID; ix += 1) {
      int v = iy * GRID + ix;
      poster_base[v * 3] = (float)(ix * FILM_WIDTH / (GRID - 1) - FILM_WIDTH / 2);
      poster_base[v * 3 + 1] = (float)(FILM_HEIGHT / 2 - iy * FILM_HEIGHT / (GRID - 1));
      poster_base[v * 3 + 2] = 0;
      // three.js uv (ix/99, 1 - iy/99) on a flipY upload is this on an
      // image stored top row first.
      m.texcoords[v * 2] = (float)ix / (GRID - 1);
      m.texcoords[v * 2 + 1] = (float)iy / (GRID - 1);
    }
  memcpy(m.vertices, poster_base, sizeof poster_base);
  int k = 0;
  for (int iy = 0; iy < GRID - 1; iy += 1)
    for (int ix = 0; ix < GRID - 1; ix += 1) {
      unsigned short a = (unsigned short)(ix + GRID * iy), b = (unsigned short)(ix + GRID * (iy + 1));
      unsigned short c = (unsigned short)(ix + 1 + GRID * (iy + 1)), d = (unsigned short)(ix + 1 + GRID * iy);
      m.indices[k++] = a; m.indices[k++] = b; m.indices[k++] = d;
      m.indices[k++] = b; m.indices[k++] = c; m.indices[k++] = d;
    }
  UploadMesh(&m, true);
  poster_mesh = m;
}

// The film poster is a folded sheet hanging from its top edge: five
// accordion panels down its height, alternately valley and mountain folds,
// and one soft crease down the middle. `value` (0.08 to 1.4, from scroll)
// sets how far the sheet is drawn up: its folded height is a fraction
// 1 - POSTER_DRAW_UP * value of the flat height, never below POSTER_MIN_DROP.
// The paper does not stretch, so a panel of length L folded to cover
// L cos(angle) of height stands out L sin(angle) in depth. The sheet draws
// up toward its top edge as it folds and drops open as the reader scrolls
// down to it. Folded, its lower panels also lean out
// toward the viewer, as a sheet held only at its top edge curls.
#define POSTER_PANELS 5
#define POSTER_DRAW_UP 0.63     // tuned to the previous build's folded height
#define POSTER_MIN_DROP 0.12    // never quite flat against itself
#define POSTER_LEAN 6.0         // forward lean of the bottom edge per unit of deformation
void poster_set_deformation(double value) {
  double next = clampd(value, 0.08, 1.4);
  if (!isnan(P.poster_deformation) && fabs(next - P.poster_deformation) < 0.00001) return;
  P.poster_deformation = next;
  double drop = fmax(1 - POSTER_DRAW_UP * next, POSTER_MIN_DROP);
  double panel = FILM_HEIGHT / POSTER_PANELS, depth = panel * sqrt(1 - drop * drop);
  for (int iy = 0; iy < GRID; iy += 1) {
    double along = iy * (double)POSTER_PANELS / (GRID - 1);  // 0..5 panels down the sheet
    int index = (int)fmin(floor(along), POSTER_PANELS - 1);
    double t = along - index;
    double zigzag = index % 2 == 0 ? 1 - t : t;  // 1 at a mountain fold (the top edge), 0 at a valley
    double y = FILM_HEIGHT / 2 - along * panel * drop;
    for (int ix = 0; ix < GRID; ix += 1) {
      int v = iy * GRID + ix;
      double across = (double)ix / (GRID - 1);
      double crease = 1 - fabs(2 * across - 1);  // a tent along the middle crease
      double wave = sin(ix * 0.13 + iy * 0.07) * sin(iy * 0.05);
      poster_mesh.vertices[v * 3 + 1] = (float)y;
      poster_mesh.vertices[v * 3 + 2] = (float)(depth * zigzag + POSTER_LEAN * next * along / POSTER_PANELS
                                                 + 0.9 * next * crease + 0.25 * next * wave);
    }
  }
  UpdateMeshBuffer(poster_mesh, 0, poster_mesh.vertices, sizeof(float) * 3 * GRID * GRID, 0);
}

static M4 poster_world(void) { return m4_compose(P.poster_position, P.poster_rotation, 1); }

// Every deformed vertex: folded interior rows can reach past the corners.
Bounds poster_bounds(void) {
  if (!P.poster_visible) return (Bounds){0};
  M4 m = m4_mul(P.view_projection, poster_world());
  double width = P.in.viewport_width, height = P.in.viewport_height;
  double left = INFINITY, right = -INFINITY, top = INFINITY, bottom = -INFINITY;
  for (int v = 0; v < GRID * GRID; v += 1) {
    V3 p = m4_project(m, v3(poster_mesh.vertices[v * 3], poster_mesh.vertices[v * 3 + 1], poster_mesh.vertices[v * 3 + 2]));
    if (p.z < -1 || p.z > 1) continue;
    double x = (p.x + 1) * width * 0.5, y = (1 - p.y) * height * 0.5;
    left = fmin(left, x); right = fmax(right, x); top = fmin(top, y); bottom = fmax(bottom, y);
  }
  if (!isfinite(left) || right <= 0 || bottom <= 0 || left >= width || top >= height) return (Bounds){0};
  return (Bounds){left, top, right - left, bottom - top, true};
}

// ── setup and textures ─────────────────────────────────────────────────────

bool render_init(void) {
  book_mesh = book_generate_mesh();
  if (book_mesh.vertexCount < 1) return false;
  BoundingBox box = GetMeshBoundingBox(book_mesh);
  book_bounds_min[0] = box.min.x; book_bounds_min[1] = box.min.y; book_bounds_min[2] = box.min.z;
  book_bounds_max[0] = box.max.x; book_bounds_max[1] = box.max.y; book_bounds_max[2] = box.max.z;
  book_shader = load_shader("shaders/book.vs", "shaders/book.fs");
  basic_shader = load_shader("shaders/basic.vs", "shaders/basic.fs");
  if (!IsShaderValid(book_shader) || !IsShaderValid(basic_shader)) return false;
  book_material = LoadMaterialDefault();
  book_material.shader = book_shader;
  basic_material = LoadMaterialDefault();
  basic_material.shader = basic_shader;
  create_poster_mesh();
  book_generate_maps();
  return true;
}

// RGBA8 pixels, top row first (from the page, or generated). The book mesh's
// UVs have v up (press_book.c), so the book maps are flipped to match; the
// poster and disc meshes carry UVs for a top-first image instead.
void render_upload_texture(int id, const unsigned char *rgba, int width, int height) {
  if (id < 0 || id >= TEX_COUNT) return;
  Image image = {MemAlloc((unsigned)(width * height * 4)), width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  memcpy(image.data, rgba, (size_t)width * height * 4);
  bool book_map = id <= TEX_GLITTER || id >= TEX_BOOK_BASE;
  if (book_map) ImageFlipVertical(&image);
  if (textures[id].id) UnloadTexture(textures[id]);
  textures[id] = LoadTextureFromImage(image);
  UnloadImage(image);
  GenTextureMipmaps(&textures[id]);
  SetTextureFilter(textures[id], TEXTURE_FILTER_TRILINEAR);
  SetTextureFilter(textures[id], id >= TEX_DISC_0 && id <= TEX_DISC_2 ? TEXTURE_FILTER_ANISOTROPIC_4X
                                                                    : id == TEX_POSTER ? TEXTURE_FILTER_ANISOTROPIC_16X
                                                                                       : TEXTURE_FILTER_ANISOTROPIC_8X);
  SetTextureWrap(textures[id], TEXTURE_WRAP_CLAMP);
  if (id == TEX_POSTER) P.poster_ready = true;
}

Texture2D render_texture(int id) { return textures[id]; }

// ── drawing ────────────────────────────────────────────────────────────────

static void bind(int unit, Shader s, const char *sampler, Texture2D t) {
  SetShaderValue(s, loc(s, sampler), &unit, SHADER_UNIFORM_INT);
  rlActiveTextureSlot(unit);
  rlEnableTexture(t.id);
}

static void set_book_lights(void) {
  Shader s = book_shader;
  // createCleanRoomLightRig: ambient 0.52, key 0.6 from (4, 9.5, 4.5), back
  // from (-32, 12, -16) and the rake spot, all scaled by LEGACY_LIGHT_SCALE.
  set_v3(s, "ambientLightColor", v3(0.52 * LEGACY_LIGHT_SCALE, 0.52 * LEGACY_LIGHT_SCALE, 0.52 * LEGACY_LIGHT_SCALE));
  float directions[6], colors[6];
  V3 key = m4_direction(P.view, v3(4, 9.5, 4.5)), back = m4_direction(P.view, v3(-32, 12, -16));
  directions[0] = (float)key.x; directions[1] = (float)key.y; directions[2] = (float)key.z;
  directions[3] = (float)back.x; directions[4] = (float)back.y; directions[5] = (float)back.z;
  double k = 0.6 * LEGACY_LIGHT_SCALE, b = 0.5 * LEGACY_LIGHT_SCALE;
  colors[0] = colors[1] = colors[2] = (float)k;
  colors[3] = (float)(P.back_color.r * b); colors[4] = (float)(P.back_color.g * b); colors[5] = (float)(P.back_color.b * b);
  SetShaderValueV(s, loc(s, "directionalDirection"), directions, SHADER_UNIFORM_VEC3, 2);
  SetShaderValueV(s, loc(s, "directionalColor"), colors, SHADER_UNIFORM_VEC3, 2);
  set_v3(s, "spotPosition", m4_point(P.view, P.rake_position));
  V3 d = v3(P.rake_position.x - P.rake_target.x, P.rake_position.y - P.rake_target.y, P.rake_position.z - P.rake_target.z);
  set_v3(s, "spotDirection", m4_direction(P.view, d));
  double rake = 0xe4 / 255.0 * P.rake_intensity;
  set_v3(s, "spotColor", v3(rake, rake, rake));
  set_f(s, "spotConeCos", cos(0.36));
  set_f(s, "spotPenumbraCos", cos(0.36 * (1 - 1)));
}

static void draw_book(int i) {
  const Book *b = &P.books[i];
  Shader s = book_shader;
  set_f(s, "thickness", b->thickness);
  set_rgb(s, "specular", b->specular, 1);
  set_f(s, "shininess", b->shininess);
  set_f(s, "reflectiveness", b->reflectiveness);
  set_rgb(s, "diffuseBaseColor", b->diffuse_base, 1);
  set_f(s, "bumpScaleBase", b->bump_base);
  set_f(s, "bumpScaleCustom", b->bump_custom);
  set_f(s, "foilDetail", b->foil_detail);
  set_f(s, "foilOpacity", b->foil_opacity);
  set_f(s, "foilSpecular", b->foil_specular);
  set_f(s, "foilEmissive", b->foil_emissive);
  set_f(s, "glossOpacity", b->gloss_opacity);
  set_f(s, "glossSpecular", b->gloss_specular);
  set_f(s, "glossEmissive", b->gloss_emissive);
  set_f(s, "glitterOpacity", b->glitter_opacity);
  set_f(s, "glitterSpecular", b->glitter_specular);
  set_f(s, "glitterEmissive", b->glitter_emissive);
  int base = TEX_BOOK_BASE + i * 4;
  bind(1, s, "diffuseMapCustom", textures[base]);
  bind(2, s, "bumpMapBase", textures[TEX_BUMP_NONE + b->base_bump]);
  bind(3, s, "bumpMapCustom", textures[base + 1]);
  bind(4, s, "foilMap", textures[base + 2]);
  bind(5, s, "glossMap", textures[base + 3]);
  bind(6, s, "glitterMap", textures[TEX_GLITTER]);
  // DrawMesh binds unit 0 from the material.
  SetShaderValue(s, loc(s, "diffuseMapBase"), &(int){0}, SHADER_UNIFORM_INT);
  book_material.maps[MATERIAL_MAP_DIFFUSE].texture = textures[TEX_SHARED_OVERLAY];
  DrawMesh(book_mesh, book_material, to_rl(book_world_matrix(b)));
}

static void draw_poster(void) {
  Shader s = basic_shader;
  set_v3(s, "diffuse", v3(1, 1, 1));
  set_f(s, "opacity", 1);
  set_f(s, "useMap", 1);
  SetShaderValue(s, loc(s, "map"), &(int){0}, SHADER_UNIFORM_INT);
  basic_material.maps[MATERIAL_MAP_DIFFUSE].texture = textures[TEX_POSTER];
  // A transparent, double-sided MeshBasicMaterial with NormalBlending.
  rlDrawRenderBatchActive();
  rlEnableColorBlend();
  rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
  rlSetBlendMode(BLEND_CUSTOM_SEPARATE);
  rlDisableBackfaceCulling();
  DrawMesh(poster_mesh, basic_material, to_rl(poster_world()));
  rlDrawRenderBatchActive();
  rlEnableBackfaceCulling();
  rlSetBlendMode(BLEND_ALPHA);
  rlDisableColorBlend();
}

void render_frame(void) {
  double dpr = P.in.device_pixel_ratio > 0 ? fmin(P.in.device_pixel_ratio, 2) : 1;
  int width = (int)round(P.in.viewport_width * dpr), height = (int)round(P.in.viewport_height * dpr);
  if (width != framebuffer_width || height != framebuffer_height) {
    framebuffer_width = width;
    framebuffer_height = height;
    SetWindowSize(width, height);
  }
  BeginDrawing();
  rlViewport(0, 0, width, height);
  // setClearColor(backdrop, holdBackdrop) on a premultiplied-alpha canvas.
  double a = P.out.backdrop_alpha;
  rlClearColor((unsigned char)round(P.backdrop.r * a * 255), (unsigned char)round(P.backdrop.g * a * 255),
               (unsigned char)round(P.backdrop.b * a * 255), (unsigned char)round(a * 255));
  rlClearScreenBuffers();
  rlDrawRenderBatchActive();
  rlSetMatrixProjection(to_rl(P.projection));
  rlSetMatrixModelview(to_rl(P.view));
  rlEnableDepthTest();
  rlEnableDepthMask();
  rlEnableBackfaceCulling();
  // The book material is opaque: no blending, its alpha is kept for the page.
  rlDisableColorBlend();
  set_book_lights();
  for (int i = 0; i < P.count; i += 1)
    if (P.books[i].visible) draw_book(i);
  if (P.poster_visible) draw_poster();
  rlEnableColorBlend();
  signal_render();
  EndDrawing();
}
