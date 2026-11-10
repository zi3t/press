// The book: its mesh and its shared surface maps, generated in code.
//
// Nothing here is loaded from a file. The geometry (the case: one sheet of
// cloth-covered board bent round the book with every edge rolled; the text
// block fitted inside it; two endbands) and the maps (a shading overlay laid out for this mesh's own UVs,
// four cloth/paper relief maps and a glitter tile) are built when the module
// starts, so their provenance is this source file.
//
// Units are centimetres. y is the book's height, x its thickness (the front
// board faces +x) and z its width, with the spine at +z. UVs place each part
// in the 1920 x 1600 cover atlas that textures.ts paints: the cloth wrap
// (back board, spine, front board) fills rows 320-1600 and the page edges,
// pastedown and endband share the rows above, clear of the foil palette tile
// in the top-left corner.
#include <stdlib.h>
#include <string.h>

#include "raylib.h"

#include "press.h"

#define ATLAS_W 1920.0
#define ATLAS_H 1600.0

// The case, from first principles. A cased book's cover is one sheet: board
// and spine stiffener glued to one piece of cloth that wraps their edges. So
// it is modelled as one sheet of thickness 2 * CASE_R, bent along a single
// cross-section curve (back board, joint, spine, joint, front board), swept
// up the height, with every exposed edge rolled round at radius CASE_R. The
// joint is the French groove where the cover hinges; the spine stands just
// inside the boards' planes with rounded shoulders.
//
// Units are centimetres. y is the book's height, x its thickness (the front
// board faces +x) and z its width, with the spine at +z. The outer extents
// (BOOK_MODEL_THICKNESS = 3.374 thick) are what book.vs scales along x.
#define HALF_HEIGHT 11.84
#define HALF_THICKNESS 1.687   // BOOK_MODEL_THICKNESS / 2
#define FORE_EDGE (-7.95)      // outer fore edge of the boards
#define SPINE_OUTER 7.96       // outer face of the spine
#define CASE_R 0.12            // half the board's thickness (2.4 mm board)
#define BOARD_X (HALF_THICKNESS - CASE_R)        // mid-surface plane of the front board
#define CASE_HALF_HEIGHT (HALF_HEIGHT - CASE_R)  // mid-surface head
#define CASE_FORE (FORE_EDGE + CASE_R)           // mid-surface fore edge
#define SPINE_FACE (SPINE_OUTER - CASE_R)        // mid-surface spine face
#define JOINT_START 6.72       // z where the board ends and the joint begins
#define JOINT_END 7.50         // z where it rises onto the spine's shoulder (8 mm French joint)
#define JOINT_DEPTH 0.13       // how far the cloth sinks into the joint
#define SHOULDER_INSET 0.025   // the spine stands just inside the boards' planes
#define SPINE_CORNER 0.24      // radius of the spine's shoulders (mid-surface)
#define SQUARE 0.30            // board overhang past the text block

// Text block: set in by the squares, fitted inside the case.
#define PAGE_HALF_HEIGHT (HALF_HEIGHT - SQUARE)
#define PAGE_FORE (FORE_EDGE + SQUARE)
#define PAGE_SPINE (SPINE_FACE - CASE_R - 0.03)
#define PAGE_HALF_THICKNESS (BOARD_X - CASE_R - 0.004)

// Atlas columns, in atlas pixels from the left (textures.ts paints the cover
// and spine type at about 53 px/cm): the back cover (board and joint) runs
// from BACK_LEFT to the spine's shoulder, the spine face is centred on
// SPINE_CENTRE, and the front mirrors the back.
#define WRAP_TOP 356.0
#define WRAP_BOTTOM 1568.0
#define PX_PER_CM 53.0
#define BACK_LEFT 36.0
#define FRONT_RIGHT 1884.0
#define SPINE_LEFT 874.0
#define SPINE_RIGHT 1046.0
#define SPINE_CENTRE ((SPINE_LEFT + SPINE_RIGHT) / 2)
#define SHOULDER_GAP 6.0       // px between a cover's edge and the spine face's edge, over the shoulder
// A cloth patch below the wrap's bottom margin that nothing prints on; the
// case's inner skin (turn-ins and lining, seen only past the squares) uses it.
#define TURN_IN ((Px){1903, 1590})

typedef struct { float *v, *n, *t; unsigned short *i; int vc, ic, cap_v, cap_i; } Builder;

static void builder_init(Builder *b, int vertices, int indices) {
  memset(b, 0, sizeof *b);
  b->v = MemAlloc(sizeof(float) * 3 * vertices);
  b->n = MemAlloc(sizeof(float) * 3 * vertices);
  b->t = MemAlloc(sizeof(float) * 2 * vertices);
  b->i = MemAlloc(sizeof(unsigned short) * indices);
  b->cap_v = vertices;
  b->cap_i = indices;
}

// Atlas pixel to three.js-style uv (v up), which book.vs uses as given.
typedef struct { double x, y; } Px;

static int vertex(Builder *b, V3 p, V3 n, Px px) {
  if (b->vc >= b->cap_v) return 0;
  int k = b->vc++;
  b->v[k * 3] = (float)p.x; b->v[k * 3 + 1] = (float)p.y; b->v[k * 3 + 2] = (float)p.z;
  b->n[k * 3] = (float)n.x; b->n[k * 3 + 1] = (float)n.y; b->n[k * 3 + 2] = (float)n.z;
  b->t[k * 2] = (float)(px.x / ATLAS_W);
  b->t[k * 2 + 1] = (float)(1 - px.y / ATLAS_H);
  return k;
}

static V3 add(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static V3 sub(V3 a, V3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static V3 scale(V3 a, double k) { return v3(a.x * k, a.y * k, a.z * k); }
static V3 cross(V3 a, V3 b) { return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static double dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static V3 unit(V3 a) { double l = sqrt(dot(a, a)); return l > 0 ? v3(a.x / l, a.y / l, a.z / l) : a; }

// Two triangles over vertices a b c d (in order round a quad), wound so they
// face along `outward`.
static void triangles(Builder *b, int a, int c1, int c2, int d, V3 outward) {
  if (b->ic + 6 > b->cap_i) return;
  V3 pa = v3(b->v[a * 3], b->v[a * 3 + 1], b->v[a * 3 + 2]);
  V3 p1 = v3(b->v[c1 * 3], b->v[c1 * 3 + 1], b->v[c1 * 3 + 2]);
  V3 p2 = v3(b->v[c2 * 3], b->v[c2 * 3 + 1], b->v[c2 * 3 + 2]);
  V3 pd = v3(b->v[d * 3], b->v[d * 3 + 1], b->v[d * 3 + 2]);
  V3 n = cross(sub(p1, pa), sub(p2, pa));
  if (dot(n, n) < 1e-14) n = cross(sub(p2, pa), sub(pd, pa));  // a corner collapsed on a rolled edge
  bool flip = dot(n, outward) < 0;
  unsigned short q[6] = {(unsigned short)a, (unsigned short)c1, (unsigned short)c2, (unsigned short)a, (unsigned short)c2, (unsigned short)d};
  if (flip) { q[1] = (unsigned short)c2; q[2] = (unsigned short)c1; q[4] = (unsigned short)d; q[5] = (unsigned short)c2; }
  memcpy(b->i + b->ic, q, sizeof q);
  b->ic += 6;
}

// A flat quad p0 p1 p2 p3 (in order around it) facing `outward`.
static void quad(Builder *b, V3 p0, V3 p1, V3 p2, V3 p3, Px t0, Px t1, Px t2, Px t3, V3 outward) {
  V3 n = unit(cross(sub(p1, p0), sub(p2, p0)));
  if (dot(n, outward) < 0) n = v3(-n.x, -n.y, -n.z);
  int a = vertex(b, p0, n, t0), c1 = vertex(b, p1, n, t1), c2 = vertex(b, p2, n, t2), d = vertex(b, p3, n, t3);
  triangles(b, a, c1, c2, d, outward);
}

static double row_px(double y) { return mixd(WRAP_TOP, WRAP_BOTTOM, (HALF_HEIGHT - y) / (2 * HALF_HEIGHT)); }

// ── the case's cross-section ───────────────────────────────────────────────

#define CURVE_MAX 160
typedef struct {
  V3 c[CURVE_MAX], t[CURVE_MAX], n[CURVE_MAX];  // mid-surface point, tangent, outward normal (y = 0)
  double s[CURVE_MAX], u[CURVE_MAX];            // arc length, atlas column on the outer face
  int count;
  int face_first, face_last;                    // the nodes on the flat of the spine
} Curve;
static Curve curve;

// The front half of the mid-surface, from the fore edge to the middle of the
// spine (x >= 0); the back half mirrors it.
static int front_half(V3 *out) {
  int n = 0;
  for (int k = 0; k <= 8; k += 1) out[n++] = v3(BOARD_X, 0, mixd(CASE_FORE, JOINT_START, k / 8.0));
  // The joint: the cloth sinks into the groove in a smooth dip and rises
  // onto the shoulder, which stands SHOULDER_INSET inside the board's plane.
  for (int k = 1; k <= 18; k += 1) {
    double t = k / 18.0;
    double dip = JOINT_DEPTH * 0.5 * (1 - cos(2 * M_PI * t)) + SHOULDER_INSET * smooth01(t);
    out[n++] = v3(BOARD_X - dip, 0, mixd(JOINT_START, JOINT_END, t));
  }
  double shoulder_x = BOARD_X - SHOULDER_INSET, corner_z = SPINE_FACE - SPINE_CORNER;
  out[n++] = v3(shoulder_x, 0, corner_z);
  // The shoulder: a quarter round onto the spine's face.
  for (int k = 1; k <= 10; k += 1) {
    double a = M_PI / 2 * k / 10.0;
    out[n++] = v3(shoulder_x - SPINE_CORNER + SPINE_CORNER * cos(a), 0, corner_z + SPINE_CORNER * sin(a));
  }
  for (int k = 1; k <= 6; k += 1) out[n++] = v3((shoulder_x - SPINE_CORNER) * (1 - k / 6.0), 0, SPINE_FACE);
  return n;
}

static void build_curve(void) {
  V3 half[64];
  int h = front_half(half);
  Curve *c = &curve;
  c->count = 0;
  // Back fore edge round to the spine's middle (mirrored), then out to the front fore edge.
  for (int k = 0; k < h; k += 1) c->c[c->count++] = v3(-half[k].x, 0, half[k].z);
  for (int k = h - 2; k >= 0; k -= 1) c->c[c->count++] = half[k];
  c->s[0] = 0;
  for (int k = 1; k < c->count; k += 1) c->s[k] = c->s[k - 1] + sqrt(dot(sub(c->c[k], c->c[k - 1]), sub(c->c[k], c->c[k - 1])));
  for (int k = 0; k < c->count; k += 1) {
    V3 d = sub(c->c[k + 1 < c->count ? k + 1 : k], c->c[k > 0 ? k - 1 : k]);
    c->t[k] = unit(d);
    c->n[k] = v3(-c->t[k].z, 0, c->t[k].x);  // outward: -x on the back board, +z on the spine
  }
  // Atlas columns. The flat of the spine maps linearly in x round SPINE_CENTRE
  // (so its type keeps one width); each cover (board and joint) maps linearly
  // in arc length from its fore edge to its shoulder; the shoulders take the
  // few pixels between.
  double flat = BOARD_X - SHOULDER_INSET - SPINE_CORNER;
  c->face_first = h - 7;
  c->face_last = c->count - 1 - (h - 7);
  int shoulder_back = c->face_first - 10, shoulder_front = c->face_last + 10;
  double spine_px = (SPINE_RIGHT - SPINE_LEFT) / (2 * HALF_THICKNESS);
  double face_left = SPINE_CENTRE - flat * spine_px, face_right = SPINE_CENTRE + flat * spine_px;
  for (int k = 0; k < c->count; k += 1) {
    if (k <= shoulder_back) c->u[k] = mixd(BACK_LEFT, face_left - SHOULDER_GAP, c->s[k] / c->s[shoulder_back]);
    else if (k < c->face_first) c->u[k] = mixd(face_left - SHOULDER_GAP, face_left, (c->s[k] - c->s[shoulder_back]) / (c->s[c->face_first] - c->s[shoulder_back]));
    else if (k <= c->face_last) c->u[k] = SPINE_CENTRE + c->c[k].x * spine_px;
    else if (k < shoulder_front) c->u[k] = mixd(face_right, face_right + SHOULDER_GAP, (c->s[k] - c->s[c->face_last]) / (c->s[shoulder_front] - c->s[c->face_last]));
    else c->u[k] = mixd(face_right + SHOULDER_GAP, FRONT_RIGHT, (c->s[k] - c->s[shoulder_front]) / (c->s[c->count - 1] - c->s[shoulder_front]));
  }
}

// The mid-surface at arc length s, extended straight past either end.
static void curve_at(double s, V3 *point, V3 *tangent, V3 *normal) {
  const Curve *c = &curve;
  int last = c->count - 1;
  if (s <= 0) { *tangent = c->t[0]; *normal = c->n[0]; *point = add(c->c[0], scale(c->t[0], s)); return; }
  if (s >= c->s[last]) { *tangent = c->t[last]; *normal = c->n[last]; *point = add(c->c[last], scale(c->t[last], s - c->s[last])); return; }
  int k = 0;
  while (k + 1 < last && c->s[k + 1] < s) k += 1;
  double f = (s - c->s[k]) / (c->s[k + 1] - c->s[k]);
  *point = v3_lerp(c->c[k], c->c[k + 1], f);
  *tangent = unit(v3_lerp(c->t[k], c->t[k + 1], f));
  *normal = unit(v3_lerp(c->n[k], c->n[k + 1], f));
}

static double curve_u(double s) {
  const Curve *c = &curve;
  int last = c->count - 1;
  if (s <= 0) return c->u[0];
  if (s >= c->s[last]) return c->u[last];
  int k = 0;
  while (k + 1 < last && c->s[k + 1] < s) k += 1;
  return mixd(c->u[k], c->u[k + 1], (s - c->s[k]) / (c->s[k + 1] - c->s[k]));
}

// ── the case ───────────────────────────────────────────────────────────────
//
// Built as a rounded slab in the sheet's own coordinates (s along the curve,
// y up the height, w through the thickness): lay a grid over the faces of the
// box [-R, L+R] x [-Y-R, Y+R] x [-R, R], pull each point onto the surface R
// from the sheet [0, L] x [-Y, Y] x {0}, which rolls every edge, then bend it
// into place: point = curve(s) + w * normal(s) + y.

#define ROLL_STEPS 5

// Grid coordinates across a rolled edge, from the slab's face (offset 0) to
// the sheet's edge (offset R), spaced so the projected points sweep the arc
// evenly.
static double roll_offset(int k) { return CASE_R * (1 - tan(M_PI / 4 * (1 - (double)k / ROLL_STEPS))); }

static int axis_with_rolls(double lo, double hi, const double *interior, int count, double *out) {
  int n = 0;
  for (int k = 0; k < ROLL_STEPS; k += 1) out[n++] = lo - CASE_R + roll_offset(k);
  for (int k = 0; k < count; k += 1) out[n++] = interior[k];
  for (int k = ROLL_STEPS - 1; k >= 0; k -= 1) out[n++] = hi + CASE_R - roll_offset(k);
  return n;
}

// Atlas spot for a slab-box point (before rolling), by face: the outer face
// prints the cover; the rolled edges carry the cloth on past the cover's
// edge, as if unfolded; the inner skin is the shaded turn-in.
static Px case_px(V3 p, int face_axis, int face_side) {
  double s = p.x, y = p.y, w = p.z, L = curve.s[curve.count - 1];
  double round = (CASE_R - w) * PX_PER_CM;  // distance round the roll from the outer face
  if (face_axis == 2) return face_side > 0 ? (Px){curve_u(s), row_px(y)} : TURN_IN;
  if (face_axis == 1) return (Px){curve_u(s), face_side > 0 ? row_px(HALF_HEIGHT) - round : row_px(-HALF_HEIGHT) + round};
  if (face_side < 0) return (Px){curve_u(0) - round, row_px(y)};
  (void)L;
  return (Px){curve_u(L) + round, row_px(y)};
}

static void case_shell(Builder *b) {
  build_curve();
  double L = curve.s[curve.count - 1], Y = CASE_HALF_HEIGHT;
  double samples[3][CURVE_MAX + 2 * ROLL_STEPS + 4];
  int count[3];
  count[0] = axis_with_rolls(0, L, curve.s, curve.count, samples[0]);
  double ys[] = {-Y, -Y / 2, 0, Y / 2, Y};
  count[1] = axis_with_rolls(-Y, Y, ys, 5, samples[1]);
  double ws[] = {0};
  count[2] = axis_with_rolls(0, 0, ws, 1, samples[2]);
  double lo[3] = {-CASE_R, -Y - CASE_R, -CASE_R}, hi[3] = {L + CASE_R, Y + CASE_R, CASE_R};
  double in_lo[3] = {0, -Y, 0}, in_hi[3] = {L, Y, 0};
  for (int fa = 0; fa < 3; fa += 1)
    for (int side = -1; side <= 1; side += 2) {
      int ua = (fa + 1) % 3, va = (fa + 2) % 3;
      int base = b->vc;
      double pc[3];
      pc[fa] = side > 0 ? hi[fa] : lo[fa];
      for (int i = 0; i < count[ua]; i += 1)
        for (int j = 0; j < count[va]; j += 1) {
          pc[ua] = samples[ua][i];
          pc[va] = samples[va][j];
          double d[3], len = 0;
          for (int a = 0; a < 3; a += 1) { d[a] = pc[a] - clampd(pc[a], in_lo[a], in_hi[a]); len += d[a] * d[a]; }
          len = sqrt(len);
          double q[3];
          for (int a = 0; a < 3; a += 1) q[a] = pc[a] - d[a] + d[a] / len * CASE_R;  // onto the roll
          V3 point, tangent, normal;
          curve_at(q[0], &point, &tangent, &normal);
          V3 position = add(add(point, scale(normal, q[2])), v3(0, q[1], 0));
          V3 n = unit(add(add(scale(tangent, d[0] / len), v3(0, d[1] / len, 0)), scale(normal, d[2] / len)));
          vertex(b, position, n, case_px(v3(pc[0], pc[1], pc[2]), fa, side));
        }
      for (int i = 0; i + 1 < count[ua]; i += 1)
        for (int j = 0; j + 1 < count[va]; j += 1) {
          int k00 = base + i * count[va] + j, k01 = k00 + 1, k10 = k00 + count[va], k11 = k10 + 1;
          V3 outward = v3(b->n[k00 * 3] + b->n[k11 * 3], b->n[k00 * 3 + 1] + b->n[k11 * 3 + 1], b->n[k00 * 3 + 2] + b->n[k11 * 3 + 2]);
          triangles(b, k00, k10, k11, k01, outward);
        }
    }
}

// ── text block ─────────────────────────────────────────────────────────────
//
// The pages, set in from the case's edges by the squares and fitted inside
// it: toward the spine they narrow to follow the case's inner skin through
// the joints and shoulders, as a backed text block does.

#define PAGE_STEPS 40
static double page_half_width(double z) {
  // The front half of the case's inner skin, x as a function of z (the spine
  // face excluded, where z is constant).
  const Curve *c = &curve;
  double best = PAGE_HALF_THICKNESS;
  for (int k = c->face_last; k + 1 < c->count; k += 1) {
    V3 a = sub(c->c[k], scale(c->n[k], CASE_R)), d = sub(c->c[k + 1], scale(c->n[k + 1], CASE_R));
    double z0 = fmin(a.z, d.z), z1 = fmax(a.z, d.z);
    if (z < z0 || z > z1 || z1 - z0 < 1e-9) continue;
    double x = mixd(a.x, d.x, (z - a.z) / (d.z - a.z));
    best = fmin(best, x - 0.004);
  }
  return fmax(best, 0.05);
}

static void pages(Builder *b) {
  double h = PAGE_HALF_HEIGHT, zs[PAGE_STEPS + 1];
  // Flat across the boards, finely sampled where the joint narrows the block.
  double bend = JOINT_START - 0.15;
  zs[0] = PAGE_FORE;
  for (int k = 1; k <= PAGE_STEPS; k += 1) zs[k] = mixd(bend, PAGE_SPINE, (double)(k - 1) / (PAGE_STEPS - 1));
  for (int k = 0; k < PAGE_STEPS; k += 1) {
    double z0 = zs[k], z1 = zs[k + 1], w0 = page_half_width(z0), w1 = page_half_width(z1);
    double u0 = mixd(1270, 640, (z0 - PAGE_FORE) / (PAGE_SPINE - PAGE_FORE)), u1 = mixd(1270, 640, (z1 - PAGE_FORE) / (PAGE_SPINE - PAGE_FORE));
    double b0 = u0 + 640, b1 = u1 + 640;
    double vt0 = 100 - 60 * w0 / PAGE_HALF_THICKNESS, vt1 = 100 + 60 * w0 / PAGE_HALF_THICKNESS;  // thickness across 40..160
    double vu0 = 100 - 60 * w1 / PAGE_HALF_THICKNESS, vu1 = 100 + 60 * w1 / PAGE_HALF_THICKNESS;
    // Head and tail: width along the atlas rect, thickness down it.
    quad(b, v3(-w0, h, z0), v3(w0, h, z0), v3(w1, h, z1), v3(-w1, h, z1),
         (Px){u0, vt0}, (Px){u0, vt1}, (Px){u1, vu1}, (Px){u1, vu0}, v3(0, 1, 0));
    quad(b, v3(-w0, -h, z0), v3(w0, -h, z0), v3(w1, -h, z1), v3(-w1, -h, z1),
         (Px){b0, vt0}, (Px){b0, vt1}, (Px){b1, vu1}, (Px){b1, vu0}, v3(0, -1, 0));
  }
  // Fore edge: height along its rect, thickness across it.
  double w = page_half_width(PAGE_FORE);
  quad(b, v3(-w, h, PAGE_FORE), v3(-w, -h, PAGE_FORE), v3(w, -h, PAGE_FORE), v3(w, h, PAGE_FORE),
       (Px){640, 200}, (Px){1910, 200}, (Px){1910, 300}, (Px){640, 300}, v3(0, 0, -1));
}

// Endbands: a small rounded bead at each end of the spine, on the swatch
// textures.ts stitches (atlas 421..616 x 197..249).
static void endbands(Builder *b) {
  const int steps = 6;
  double r = 0.11, zc = PAGE_SPINE - r, w = page_half_width(zc) * 0.97;
  for (int s = -1; s <= 1; s += 2) {
    double y0 = s * PAGE_HALF_HEIGHT;
    for (int k = 0; k < steps; k += 1) {
      double a0 = M_PI * k / steps, a1 = M_PI * (k + 1) / steps;
      V3 c0 = v3(0, y0 + s * r * sin(a0), zc - r * cos(a0)), c1 = v3(0, y0 + s * r * sin(a1), zc - r * cos(a1));
      double py0 = 197 + 52.0 * k / steps, py1 = 197 + 52.0 * (k + 1) / steps;
      V3 normal = unit(v3(0, s * sin((a0 + a1) / 2), -cos((a0 + a1) / 2)));
      quad(b, v3(-w, c0.y, c0.z), v3(w, c0.y, c0.z), v3(w, c1.y, c1.z), v3(-w, c1.y, c1.z),
           (Px){421, py0}, (Px){616, py0}, (Px){616, py1}, (Px){421, py1}, normal);
    }
  }
}

Mesh book_generate_mesh(void) {
  Builder b;
  builder_init(&b, 30000, 90000);
  case_shell(&b);
  pages(&b);
  endbands(&b);
  Mesh m = {0};
  m.vertexCount = b.vc;
  m.triangleCount = b.ic / 3;
  m.vertices = b.v; m.normals = b.n; m.texcoords = b.t; m.indices = b.i;
  UploadMesh(&m, false);
  return m;
}

// ── surface maps ───────────────────────────────────────────────────────────

// A small integer hash, so every map is reproducible.
static unsigned hash2(int x, int y, unsigned seed) {
  unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u + seed * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}
static double rand2(int x, int y, unsigned seed) { return (hash2(x, y, seed) & 0xffffff) / (double)0xffffff; }

// Smooth value noise and a few octaves of it.
static double value_noise(double x, double y, unsigned seed) {
  int xi = (int)floor(x), yi = (int)floor(y);
  double fx = x - xi, fy = y - yi;
  fx = fx * fx * (3 - 2 * fx);
  fy = fy * fy * (3 - 2 * fy);
  double a = rand2(xi, yi, seed), b = rand2(xi + 1, yi, seed), c = rand2(xi, yi + 1, seed), d = rand2(xi + 1, yi + 1, seed);
  return mixd(mixd(a, b, fx), mixd(c, d, fx), fy);
}
static double fbm(double x, double y, int octaves, unsigned seed) {
  double sum = 0, amplitude = 0.5, total = 0;
  for (int o = 0; o < octaves; o += 1) {
    sum += amplitude * value_noise(x, y, seed + (unsigned)o * 101u);
    total += amplitude;
    x *= 2.03; y *= 2.03; amplitude *= 0.5;
  }
  return sum / total;
}

static unsigned char byte(double v) { return (unsigned char)clampd(round(v), 0, 255); }

static unsigned char *atlas_pixels(void) { return MemAlloc(1920 * 1600 * 4); }
static void put(unsigned char *rgba, int width, int x, int y, double value) {
  unsigned char g = byte(value);
  unsigned char *p = rgba + ((size_t)y * width + x) * 4;
  p[0] = p[1] = p[2] = g;
  p[3] = 255;
}

static bool in_rect(int x, int y, double x0, double y0, double x1, double y1) { return x >= x0 && x < x1 && y >= y0 && y < y1; }

// The shading overlay, laid out for this mesh's UVs. 128 is neutral; darker
// values shade (the turn-in inside the case, the page edges in the shadow of
// the boards).
static void generate_overlay(unsigned char *rgba) {
  for (int y = 0; y < 1600; y += 1)
    for (int x = 0; x < 1920; x += 1) {
      double v = 128;
      if (y >= 320) {
        // Cloth: a very faint, large mottle, a little under neutral. The case's
        // own shape shades its joints and rolled edges.
        v = 120 + (fbm(x / 160.0, y / 160.0, 3, 11) - 0.5) * 4;
        // The turn-in patch (TURN_IN): cloth inside the boards, in shadow.
        if (in_rect(x, y, 1890, 1580, 1920, 1600)) v = 60;
      } else if (in_rect(x, y, 640, 40, 1910, 160) || in_rect(x, y, 640, 200, 1910, 300)) {
        // Page edges: fine sheets of slightly different shades, darker where
        // the boards overhang them (both ends of the thickness direction).
        bool fore = y >= 200;
        double across = fore ? (y - 200) : (y - 40), span = fore ? 100 : 120;
        int sheet = (int)(across / 1.6);
        v = 118 + (rand2(sheet, fore, 31) - 0.5) * 10;
        v += (fbm(x / 80.0, sheet * 0.3, 2, 7) - 0.5) * 6;
        double to_board = fmin(across, span - across);
        v -= 30 * exp(-to_board / 7);
      }
      put(rgba, 1920, x, y, v);
    }
}

// Relief maps for the cloth (rows from 320). Above that the page edges stay flat.
static double buckram(int x, int y) {
  // A plain weave: warp and weft threads crossing over and under, with the
  // uneven thickness ("slubs") of real thread so it doesn't read as a grid.
  double period = 4;
  int col = (int)floor(x / period), row = (int)floor(y / period);
  double fx = fmod(x, period) / period, fy = fmod(y, period) / period;
  bool warp_over = (col + row) % 2 == 0;
  double warp_width = 0.75 + 0.5 * fbm(col * 0.7, y / 40.0, 2, 51);
  double weft_width = 0.75 + 0.5 * fbm(x / 40.0, row * 0.7, 2, 53);
  double warp = pow(sin(fx * M_PI), 1 / warp_width), weft = pow(sin(fy * M_PI), 1 / weft_width);
  double thread = warp_over ? warp * 0.7 + weft * 0.3 : weft * 0.7 + warp * 0.3;
  double grain = (rand2(x, y, 59) - 0.5) * 10;  // loose fibres
  return 118 + (thread - 0.5) * 16 + (rand2(col, row, 3) - 0.5) * 8 + grain + (fbm(x / 90.0, y / 90.0, 2, 57) - 0.5) * 10;
}
static double paper(int x, int y) { return 112 + (fbm(x / 120.0, y / 120.0, 4, 5) - 0.5) * 120; }
static double cardboard(int x, int y) {
  double grain = (rand2(x, y, 9) - 0.5) * 14 + (fbm(x / 6.0, y / 40.0, 2, 13) - 0.5) * 12;
  // Sparse short fibres.
  double fibre = 0;
  int cx = x / 48, cy = y / 48;
  if (rand2(cx, cy, 17) > 0.82) {
    double fx = x - (cx * 48 + rand2(cx, cy, 19) * 48), fy = y - (cy * 48 + rand2(cx, cy, 23) * 48);
    double angle = rand2(cx, cy, 29) * M_PI;
    double along = fx * cos(angle) + fy * sin(angle), off = -fx * sin(angle) + fy * cos(angle);
    if (fabs(along) < 14 && fabs(off) < 0.8) fibre = -26;
  }
  return 116 + grain + fibre;
}

static void generate_relief(unsigned char *rgba, int kind) {
  for (int y = 0; y < 1600; y += 1)
    for (int x = 0; x < 1920; x += 1) {
      double v = 128;
      if (y >= 320) v = kind == 1 ? buckram(x, y) : kind == 2 ? paper(x, y) : cardboard(x, y);
      put(rgba, 1920, x, y, v);
    }
}

// The glitter tile: dim flakes with a few bright ones.
static void generate_glitter(unsigned char *rgba) {
  for (int y = 0; y < 100; y += 1)
    for (int x = 0; x < 100; x += 1) {
      double flake = rand2(x / 2, y / 2, 41);
      double v = flake > 0.86 ? 200 + rand2(x, y, 43) * 55 : 60 + flake * 110;
      put(rgba, 100, x, y, v);
    }
}

// Builds every shared map and hands it to the renderer like an uploaded image.
void book_generate_maps(void) {
  unsigned char *rgba = atlas_pixels();
  generate_overlay(rgba);
  render_upload_texture(TEX_SHARED_OVERLAY, rgba, 1920, 1600);
  for (int kind = 1; kind <= 3; kind += 1) {
    generate_relief(rgba, kind);
    render_upload_texture(TEX_BUMP_NONE + kind, rgba, 1920, 1600);
  }
  for (int i = 0; i < 64 * 64; i += 1) { rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = 128; rgba[i * 4 + 3] = 255; }
  render_upload_texture(TEX_BUMP_NONE, rgba, 64, 64);
  generate_glitter(rgba);
  render_upload_texture(TEX_GLITTER, rgba, 100, 100);
  MemFree(rgba);
}
