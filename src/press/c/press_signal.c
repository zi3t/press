// The signal medallion, ported from src/press/runtime/clean-room/signal.ts:
// its own camera and scissor window in the shared canvas, a shader backdrop,
// a metallic medallion with an orb and shine, three split discs and an
// instanced waveform. Geometry is generated exactly as three.js builds it.
#include <stdlib.h>
#include <string.h>

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include "press.h"

#if defined(PLATFORM_WEB)
#include <GLES3/gl3.h>
#endif

Texture2D render_texture(int id);

typedef struct { float *v, *n, *t; unsigned short *i; int vc, ic; } Geo;

static void geo_alloc(Geo *g, int vertices, int indices) {
  g->v = MemAlloc(sizeof(float) * 3 * vertices);
  g->n = MemAlloc(sizeof(float) * 3 * vertices);
  g->t = MemAlloc(sizeof(float) * 2 * vertices);
  g->i = MemAlloc(sizeof(unsigned short) * indices);
  g->vc = g->ic = 0;
}
static void geo_vertex(Geo *g, double x, double y, double z, double nx, double ny, double nz, double u, double v) {
  int k = g->vc++;
  g->v[k * 3] = (float)x; g->v[k * 3 + 1] = (float)y; g->v[k * 3 + 2] = (float)z;
  g->n[k * 3] = (float)nx; g->n[k * 3 + 1] = (float)ny; g->n[k * 3 + 2] = (float)nz;
  g->t[k * 2] = (float)u; g->t[k * 2 + 1] = (float)v;  // three.js uv
}
static void geo_face(Geo *g, int a, int b, int c) {
  g->i[g->ic++] = (unsigned short)a; g->i[g->ic++] = (unsigned short)b; g->i[g->ic++] = (unsigned short)c;
}
static Mesh geo_mesh(Geo *g) {
  Mesh m = {0};
  m.vertexCount = g->vc;
  m.triangleCount = g->ic / 3;
  m.vertices = g->v; m.normals = g->n; m.texcoords = g->t; m.indices = g->i;
  UploadMesh(&m, false);
  return m;
}

// RingGeometry(inner, outer, thetaSegments), one phi segment.
static Mesh ring_geometry(double inner, double outer, int segments) {
  Geo g;
  geo_alloc(&g, 2 * (segments + 1), segments * 6);
  for (int j = 0; j <= 1; j += 1) {
    double radius = inner + j * (outer - inner);
    for (int i = 0; i <= segments; i += 1) {
      double s = (double)i / segments * 2 * M_PI, x = radius * cos(s), y = radius * sin(s);
      geo_vertex(&g, x, y, 0, 0, 0, 1, (x / outer + 1) / 2, (y / outer + 1) / 2);
    }
  }
  for (int i = 0; i < segments; i += 1) {
    int a = i, b = i + segments + 1, c = i + segments + 2, d = i + 1;
    geo_face(&g, a, b, d);
    geo_face(&g, b, c, d);
  }
  return geo_mesh(&g);
}

// CircleGeometry(radius, segments). The disc faces sample canvas images
// uploaded top row first, where three.js flips them (flipY); flip v instead.
static Mesh circle_geometry(double radius, int segments) {
  Geo g;
  geo_alloc(&g, segments + 2, segments * 3);
  geo_vertex(&g, 0, 0, 0, 0, 0, 1, 0.5, 0.5);
  for (int s = 0; s <= segments; s += 1) {
    double a = (double)s / segments * 2 * M_PI, x = radius * cos(a), y = radius * sin(a);
    geo_vertex(&g, x, y, 0, 0, 0, 1, (x / radius + 1) / 2, 1 - (y / radius + 1) / 2);
  }
  for (int i = 1; i <= segments; i += 1) geo_face(&g, i, i + 1, 0);
  return geo_mesh(&g);
}

// CylinderGeometry(r, r, height, radialSegments, 1, openEnded).
static Mesh open_cylinder_geometry(double radius, double height, int segments) {
  Geo g;
  geo_alloc(&g, 2 * (segments + 1), segments * 6);
  for (int y = 0; y <= 1; y += 1)
    for (int x = 0; x <= segments; x += 1) {
      double u = (double)x / segments, theta = u * 2 * M_PI, s = sin(theta), c = cos(theta);
      geo_vertex(&g, radius * s, -y * height + height / 2, radius * c, s, 0, c, u, 1 - y);
    }
  for (int x = 0; x < segments; x += 1) {
    int a = x, b = segments + 1 + x, c = segments + 1 + x + 1, d = x + 1;
    geo_face(&g, a, b, d);
    geo_face(&g, b, c, d);
  }
  return geo_mesh(&g);
}

// TorusGeometry(radius, tube, radialSegments, tubularSegments).
static Mesh torus_geometry(double radius, double tube, int radial, int tubular) {
  Geo g;
  geo_alloc(&g, (radial + 1) * (tubular + 1), radial * tubular * 6);
  for (int j = 0; j <= radial; j += 1)
    for (int i = 0; i <= tubular; i += 1) {
      double u = (double)i / tubular * 2 * M_PI, v = (double)j / radial * 2 * M_PI;
      double x = (radius + tube * cos(v)) * cos(u), y = (radius + tube * cos(v)) * sin(u), z = tube * sin(v);
      double cx = radius * cos(u), cy = radius * sin(u);
      double nx = x - cx, ny = y - cy, nz = z, l = sqrt(nx * nx + ny * ny + nz * nz);
      geo_vertex(&g, x, y, z, nx / l, ny / l, nz / l, (double)i / tubular, (double)j / radial);
    }
  for (int j = 1; j <= radial; j += 1)
    for (int i = 1; i <= tubular; i += 1) {
      int a = (tubular + 1) * j + i - 1, b = (tubular + 1) * (j - 1) + i - 1;
      int c = (tubular + 1) * (j - 1) + i, d = (tubular + 1) * j + i;
      geo_face(&g, a, b, d);
      geo_face(&g, b, c, d);
    }
  return geo_mesh(&g);
}

// SphereGeometry(radius, widthSegments, heightSegments).
static Mesh sphere_geometry(double radius, int width, int height) {
  Geo g;
  geo_alloc(&g, (width + 1) * (height + 1), width * height * 6);
  for (int iy = 0; iy <= height; iy += 1) {
    double v = (double)iy / height;
    double u_offset = iy == 0 ? 0.5 / width : iy == height ? -0.5 / width : 0;
    for (int ix = 0; ix <= width; ix += 1) {
      double u = (double)ix / width;
      double x = -radius * cos(u * 2 * M_PI) * sin(v * M_PI), y = radius * cos(v * M_PI);
      double z = radius * sin(u * 2 * M_PI) * sin(v * M_PI);
      geo_vertex(&g, x, y, z, x / radius, y / radius, z / radius, u + u_offset, 1 - v);
    }
  }
  for (int iy = 0; iy < height; iy += 1)
    for (int ix = 0; ix < width; ix += 1) {
      int a = iy * (width + 1) + ix + 1, b = iy * (width + 1) + ix;
      int c = (iy + 1) * (width + 1) + ix, d = (iy + 1) * (width + 1) + ix + 1;
      if (iy != 0) geo_face(&g, a, b, d);
      if (iy != height - 1) geo_face(&g, b, c, d);
    }
  return geo_mesh(&g);
}

// BoxGeometry(w, h, d) without segments; only position matters here.
static Mesh box_geometry(double w, double h, double d) {
  Geo g;
  geo_alloc(&g, 24, 36);
  static const int faces[6][4][3] = {
    {{1, -1, -1}, {1, 1, -1}, {1, 1, 1}, {1, -1, 1}}, {{-1, -1, 1}, {-1, 1, 1}, {-1, 1, -1}, {-1, -1, -1}},
    {{-1, 1, -1}, {-1, 1, 1}, {1, 1, 1}, {1, 1, -1}}, {{-1, -1, 1}, {-1, -1, -1}, {1, -1, -1}, {1, -1, 1}},
    {{-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}}, {{1, -1, -1}, {-1, -1, -1}, {-1, 1, -1}, {1, 1, -1}},
  };
  for (int f = 0; f < 6; f += 1) {
    int base = g.vc;
    for (int k = 0; k < 4; k += 1)
      geo_vertex(&g, faces[f][k][0] * w / 2, faces[f][k][1] * h / 2, faces[f][k][2] * d / 2, 0, 0, 1, 0, 0);
    geo_face(&g, base, base + 1, base + 2);
    geo_face(&g, base, base + 2, base + 3);
  }
  return geo_mesh(&g);
}

// PlaneGeometry(2, 2) for the backdrop.
static Mesh plane_geometry(void) {
  Geo g;
  geo_alloc(&g, 4, 6);
  geo_vertex(&g, -1, 1, 0, 0, 0, 1, 0, 1);
  geo_vertex(&g, 1, 1, 0, 0, 0, 1, 1, 1);
  geo_vertex(&g, -1, -1, 0, 0, 0, 1, 0, 0);
  geo_vertex(&g, 1, -1, 0, 0, 0, 1, 1, 0);
  geo_face(&g, 0, 2, 1);
  geo_face(&g, 2, 3, 1);
  return geo_mesh(&g);
}

static Mesh ring, rear, rim, inner_rim, shine, orb, disc, wave_box, field;
static Shader field_shader, standard_shader, shine_shader, orb_shader, basic_shader;
static Material material;

// signal.ts state
static bool visible, did_flip;
static double flip = M_PI / 2, flip_velocity, split, time_s, entry;
static double pointer_rotation_x, pointer_rotation_y, orb_rotation_y;
static double placement_x, placement_y, placement_scale = 1;
static double camera_z = 6, aspect = 1;

static char *source(const char *path) {
  char *text = LoadFileText(path);
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
static Shader shader(const char *vs_path, const char *fs_path) {
  char *vs = source(vs_path), *fs = source(fs_path);
  Shader s = LoadShaderFromMemory(vs, fs);
  MemFree(vs);
  MemFree(fs);
  return s;
}

bool signal_init(void) {
  ring = ring_geometry(.43, 1, 112);
  rear = circle_geometry(1, 112);
  rim = open_cylinder_geometry(1, .24, 112);
  inner_rim = torus_geometry(.435, .022, 12, 96);
  shine = ring_geometry(.44, .995, 112);
  orb = sphere_geometry(.405, 48, 32);
  disc = circle_geometry(1, 96);
  wave_box = box_geometry(.009, 1, .01);
  field = plane_geometry();
  field_shader = shader("shaders/signal_field.vs", "shaders/signal_field.fs");
  standard_shader = shader("shaders/shaded.vs", "shaders/standard.fs");
  shine_shader = shader("shaders/basic.vs", "shaders/shine.fs");
  orb_shader = shader("shaders/shaded.vs", "shaders/orb.fs");
  basic_shader = shader("shaders/basic.vs", "shaders/basic.fs");
  material = LoadMaterialDefault();
  return IsShaderValid(field_shader) && IsShaderValid(standard_shader) && IsShaderValid(shine_shader)
      && IsShaderValid(orb_shader) && IsShaderValid(basic_shader);
}

// setLayout + advance. Returns whether the section is animating.
bool signal_advance(double delta) {
  const PressInput *in = &P.in;
  double width = in->viewport_width, height = in->viewport_height;
  bool detail = in->signal_detail != 0;
  visible = in->signal_bottom > 0 && in->signal_top < height && !in->reduced_motion;
  aspect = width / fmax(1, height);
  camera_z = clampd(9 / aspect, 6, 14.25);
  if (!visible) return false;

  double dt = fmin(delta, .05), frames = dt * 60;
  entry = clampd(1 - in->signal_top / height, 0, 1);
  if (!did_flip && entry >= .9) { did_flip = true; flip_velocity = -.1; }
  double target = did_flip ? 0 : M_PI * .5 * (1 - sin(entry * M_PI / 2));
  flip_velocity += (target - flip) * .02 * frames;
  flip_velocity *= pow(.88, frames);
  flip += flip_velocity * frames;
  split = damp(split, detail ? 1 : 0, 4.5, dt);
  time_s += dt;
  pointer_rotation_x = damp(pointer_rotation_x, in->signal_pointer_y * .314, 12, dt);
  pointer_rotation_y = damp(pointer_rotation_y, in->signal_pointer_x * .314, 12, dt);
  orb_rotation_y += dt * .09;
  double half_height = tan(22.5 * M_PI / 180) * camera_z;
  bool figure = detail && in->signal_figure_valid;
  double target_x = figure ? ((in->signal_figure_left + in->signal_figure_width / 2) / width - .5) * half_height * 2 * aspect : 0;
  double target_y = figure ? (.5 - (in->signal_figure_top + in->signal_figure_height / 2) / height) * half_height * 2
                           : -1.25 * (1 - sin(entry * M_PI / 2));
  placement_x = damp(placement_x, target_x, 6, dt);
  placement_y = damp(placement_y, target_y, 6, dt);
  placement_scale = damp(placement_scale, detail ? .83 : 1, 6, dt);
  return true;
}

static int loc(Shader s, const char *n) { return GetShaderLocation(s, n); }
static void set_f(Shader s, const char *n, double v) { float f = (float)v; SetShaderValue(s, loc(s, n), &f, SHADER_UNIFORM_FLOAT); }
static void set_v3(Shader s, const char *n, double x, double y, double z) {
  float f[3] = {(float)x, (float)y, (float)z};
  SetShaderValue(s, loc(s, n), f, SHADER_UNIFORM_VEC3);
}
Matrix to_rl(M4 m);
static Matrix rl(M4 m) { return to_rl(m); }

static void draw(Mesh mesh, Shader s, M4 world) {
  material.shader = s;
  DrawMesh(mesh, material, rl(world));
}

static void draw_standard(Mesh mesh, double color, double rough, double metal, M4 world, M4 view) {
  Shader s = standard_shader;
  set_v3(s, "diffuse", color, color, color);
  set_f(s, "roughness", rough);
  set_f(s, "metalness", metal);
  set_v3(s, "ambientLightColor", 1.2, 1.2, 1.2);
  V3 key = m4_direction(view, v3(-3, 4, 6)), fill = m4_direction(view, v3(4, -2, 4));
  float directions[6] = {(float)key.x, (float)key.y, (float)key.z, (float)fill.x, (float)fill.y, (float)fill.z};
  float k = (float)(0xeb / 255.0 * 2.1), f = .85f;
  float colors[6] = {k, k, k, f, f, f};
  SetShaderValueV(s, loc(s, "directionalDirection"), directions, SHADER_UNIFORM_VEC3, 2);
  SetShaderValueV(s, loc(s, "directionalColor"), colors, SHADER_UNIFORM_VEC3, 2);
  draw(mesh, s, world);
}

static M4 child(M4 parent, V3 position, Euler rotation, double scale) {
  return m4_mul(parent, m4_compose(position, rotation, scale));
}

void signal_render(void) {
  const PressInput *in = &P.in;
  if (!visible) return;
  double width = in->viewport_width, height = in->viewport_height;
  double dpr = in->device_pixel_ratio > 0 ? fmin(in->device_pixel_ratio, 2) : 1;
  double top = clampd(in->signal_top, 0, height), bottom = clampd(in->signal_bottom, 0, height);
  if (bottom - top <= 0) return;
  bool detail = in->signal_detail != 0;

  M4 view = m4_rigid_inverse(m4_compose(v3(0, 0, camera_z), (Euler){0, 0, 0, ORDER_XYZ}, 1));
  M4 projection = m4_perspective(45, aspect, .1, 100);
  rlDrawRenderBatchActive();
  rlEnableScissorTest();
  rlScissor(0, (int)round((height - bottom) * dpr), (int)round(width * dpr), (int)round((bottom - top) * dpr));
#if defined(PLATFORM_WEB)
  glClear(GL_DEPTH_BUFFER_BIT);  // renderer.clearDepth(): colour stays
#endif
  rlSetMatrixProjection(rl(projection));
  rlSetMatrixModelview(rl(view));
  rlEnableBackfaceCulling();

  // Backdrop first (renderOrder -100), without depth.
  rlDisableDepthTest();
  rlDisableDepthMask();
  rlDisableColorBlend();
  Shader fs = field_shader;
  set_f(fs, "uTime", time_s);
  set_f(fs, "uAspect", aspect);
  set_f(fs, "uEntry", entry);
  set_f(fs, "uSplit", split);
  draw(field, fs, m4_identity());
  rlDrawRenderBatchActive();
  rlEnableDepthTest();
  rlEnableDepthMask();

  M4 root = m4_identity();
  M4 placement = child(root, v3(placement_x, placement_y, 0), (Euler){0, 0, 0, ORDER_XYZ}, placement_scale);
  M4 pointer = child(placement, v3(0, 0, 0), (Euler){pointer_rotation_x, pointer_rotation_y, 0, ORDER_XYZ}, 1);
  M4 medallion = child(pointer, v3(0, 0, 0), (Euler){flip * (1 - split) - split * .13, split * -.14, split * -.08, ORDER_XYZ}, 1);
  Euler none = {0, 0, 0, ORDER_XYZ};
  bool closed_visible = split < .52;
  if (closed_visible) {
    draw_standard(ring, 0x5f / 255.0, .36, .36, child(medallion, v3(0, 0, .12), none, 1), view);
    draw_standard(rear, 0x44 / 255.0, .4, .5, child(medallion, v3(0, 0, -.12), (Euler){0, M_PI, 0, ORDER_XYZ}, 1), view);
    draw_standard(rim, 0x44 / 255.0, .4, .5, child(medallion, v3(0, 0, 0), (Euler){M_PI / 2, 0, 0, ORDER_XYZ}, 1), view);
    draw_standard(inner_rim, 0x44 / 255.0, .4, .5, child(medallion, v3(0, 0, .135), none, 1), view);
    draw(orb, orb_shader, child(medallion, v3(0, 0, .1), (Euler){0, orb_rotation_y, 0, ORDER_XYZ}, 1));
  }
  if (split >= .48) {
    rlDisableBackfaceCulling();  // DoubleSide
    for (int i = 0; i < 3; i += 1) {
      M4 group = child(medallion, v3(0, (1 - i) * 1.08 * split, i * .14 * split), none, .78);
      Shader s = basic_shader;
      set_v3(s, "diffuse", 1, 1, 1);
      set_f(s, "opacity", 1);
      set_f(s, "useMap", 1);
      material.maps[MATERIAL_MAP_DIFFUSE].texture = render_texture(TEX_DISC_0 + i);
      draw(disc, s, group);
    }
    material.maps[MATERIAL_MAP_DIFFUSE].texture = (Texture2D){0};
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
  }

  // Transparent: NormalBlending, depth tested.
  rlDrawRenderBatchActive();
  rlEnableColorBlend();
  rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
  rlSetBlendMode(BLEND_CUSTOM_SEPARATE);
  if (closed_visible) {
    rlDisableDepthMask();
    set_f(shine_shader, "uTime", time_s);
    draw(shine, shine_shader, child(medallion, v3(0, 0, .135), none, 1));
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
  }
  if (!detail) {
    Shader s = basic_shader;
    set_v3(s, "diffuse", 0xc4 / 255.0, 0xc4 / 255.0, 0xc4 / 255.0);
    set_f(s, "opacity", 1 - split);
    set_f(s, "useMap", 0);
    for (int index = 0; index < 61; index += 1) {
      double x = (index - 30) * .026;
      double h = .025 + pow(sin(index * 1.71), 2) * exp(-x * x * 3) * .24 * (.85 + .15 * sin(time_s * 1.5 + index * .35));
      M4 m = m4_compose(v3(x, -1.82 + h / 2, 0), none, 1);
      m.e[5] *= h;  // scale (1, h, 1)
      draw(wave_box, s, m);
    }
  }
  rlDrawRenderBatchActive();
  rlSetBlendMode(BLEND_ALPHA);
  rlDisableScissorTest();
}
