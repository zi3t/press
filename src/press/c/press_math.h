// three.js-compatible maths for the Press scene: Euler angles with an order,
// Euler.reorder(), Object3D matrix composition and the perspective camera.
// Matrices are column-major like three.js Matrix4.elements and raylib Matrix.
#ifndef PRESS_MATH_H
#define PRESS_MATH_H

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846  // not in strict C11
#endif

typedef struct { double x, y, z; } V3;
typedef struct { double e[16]; } M4;  // column-major: e[col * 4 + row]
typedef enum { ORDER_XYZ, ORDER_ZYX } EulerOrder;
typedef struct { double x, y, z; EulerOrder order; } Euler;

static inline V3 v3(double x, double y, double z) { return (V3){x, y, z}; }
static inline double clampd(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline double mixd(double a, double b, double t) { return a + (b - a) * t; }
static inline V3 v3_lerp(V3 a, V3 b, double t) { return v3(mixd(a.x, b.x, t), mixd(a.y, b.y, t), mixd(a.z, b.z, t)); }
static inline double v3_distance(V3 a, V3 b) { return sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z)); }

// motion.ts
static inline double damp(double from, double to, double speed, double dt) { return mixd(from, to, 1 - exp(-speed * dt)); }
static inline double frame_approach(double from, double to, double per_frame, double dt) {
  return mixd(from, to, 1 - pow(1 - per_frame, clampd(dt * 60, 0.25, 4)));
}
static inline double smooth01(double p) { double v = clampd(p, 0, 1); return v * v * (3 - 2 * v); }
static inline double wrap_rotation(double r) { return fmod(r, M_PI * 2); }

static inline M4 m4_identity(void) {
  M4 m = {{0}};
  m.e[0] = m.e[5] = m.e[10] = m.e[15] = 1;
  return m;
}

static inline M4 m4_mul(M4 a, M4 b) {  // a * b
  M4 r;
  for (int c = 0; c < 4; c += 1)
    for (int rw = 0; rw < 4; rw += 1)
      r.e[c * 4 + rw] = a.e[0 * 4 + rw] * b.e[c * 4 + 0] + a.e[1 * 4 + rw] * b.e[c * 4 + 1]
                      + a.e[2 * 4 + rw] * b.e[c * 4 + 2] + a.e[3 * 4 + rw] * b.e[c * 4 + 3];
  return r;
}

// Rotation part of Matrix4.makeRotationFromEuler, as a 3x3 R[row][col].
static inline void euler_rotation(Euler e, double R[3][3]) {
  double a = cos(e.x), b = sin(e.x), c = cos(e.y), d = sin(e.y), f = cos(e.z), g = sin(e.z);
  if (e.order == ORDER_XYZ) {
    double ae = a * f, af = a * g, be = b * f, bf = b * g;
    R[0][0] = c * f;  R[0][1] = -c * g; R[0][2] = d;
    R[1][0] = af + be * d; R[1][1] = ae - bf * d; R[1][2] = -b * c;
    R[2][0] = bf - ae * d; R[2][1] = be + af * d; R[2][2] = a * c;
  } else {  // ZYX
    double ae = a * f, af = a * g, be = b * f, bf = b * g;
    R[0][0] = c * f;  R[0][1] = be * d - af; R[0][2] = ae * d + bf;
    R[1][0] = c * g;  R[1][1] = bf * d + ae; R[1][2] = af * d - be;
    R[2][0] = -d;     R[2][1] = b * c;       R[2][2] = a * c;
  }
}

// Euler.setFromRotationMatrix for the two orders the scene uses.
static inline Euler euler_from_rotation(double R[3][3], EulerOrder order) {
  Euler e = {0, 0, 0, order};
  if (order == ORDER_XYZ) {
    e.y = asin(clampd(R[0][2], -1, 1));
    if (fabs(R[0][2]) < 0.9999999) { e.x = atan2(-R[1][2], R[2][2]); e.z = atan2(-R[0][1], R[0][0]); }
    else { e.x = atan2(R[2][1], R[1][1]); e.z = 0; }
  } else {
    e.y = asin(-clampd(R[2][0], -1, 1));
    if (fabs(R[2][0]) < 0.9999999) { e.x = atan2(R[2][1], R[2][2]); e.z = atan2(R[1][0], R[0][0]); }
    else { e.x = 0; e.z = atan2(-R[0][1], R[1][1]); }
  }
  return e;
}

// Euler.reorder(): keep the orientation, express it in another order. three.js
// goes through a quaternion; the rotation matrix gives the same angles.
static inline Euler euler_reorder(Euler e, EulerOrder order) {
  if (e.order == order) return e;
  double R[3][3];
  euler_rotation(e, R);
  return euler_from_rotation(R, order);
}

// Object3D.updateMatrix(): T * R * S.
static inline M4 m4_compose(V3 p, Euler r, double s) {
  double R[3][3];
  euler_rotation(r, R);
  M4 m = m4_identity();
  for (int row = 0; row < 3; row += 1)
    for (int col = 0; col < 3; col += 1) m.e[col * 4 + row] = R[row][col] * s;
  m.e[12] = p.x; m.e[13] = p.y; m.e[14] = p.z;
  return m;
}

static inline M4 m4_rigid_inverse(M4 m) {  // rotation + translation only
  M4 r = m4_identity();
  for (int row = 0; row < 3; row += 1)
    for (int col = 0; col < 3; col += 1) r.e[col * 4 + row] = m.e[row * 4 + col];
  for (int row = 0; row < 3; row += 1)
    r.e[12 + row] = -(r.e[0 + row] * m.e[12] + r.e[4 + row] * m.e[13] + r.e[8 + row] * m.e[14]);
  return r;
}

// PerspectiveCamera.updateProjectionMatrix (zoom 1, no film offset).
static inline M4 m4_perspective(double fov_deg, double aspect, double near, double far) {
  double top = near * tan(fov_deg * M_PI / 360), height = 2 * top, width = aspect * height;
  M4 m = {{0}};
  m.e[0] = 2 * near / width;
  m.e[5] = 2 * near / height;
  m.e[10] = -(far + near) / (far - near);
  m.e[11] = -1;
  m.e[14] = -2 * far * near / (far - near);
  return m;
}

static inline V3 m4_point(M4 m, V3 p) {
  return v3(m.e[0] * p.x + m.e[4] * p.y + m.e[8] * p.z + m.e[12],
            m.e[1] * p.x + m.e[5] * p.y + m.e[9] * p.z + m.e[13],
            m.e[2] * p.x + m.e[6] * p.y + m.e[10] * p.z + m.e[14]);
}

static inline V3 m4_direction(M4 m, V3 d) {  // Vector3.transformDirection
  V3 r = v3(m.e[0] * d.x + m.e[4] * d.y + m.e[8] * d.z, m.e[1] * d.x + m.e[5] * d.y + m.e[9] * d.z,
            m.e[2] * d.x + m.e[6] * d.y + m.e[10] * d.z);
  double l = sqrt(r.x * r.x + r.y * r.y + r.z * r.z);
  return l > 0 ? v3(r.x / l, r.y / l, r.z / l) : r;
}

// Vector3.project(camera): to normalised device coordinates.
static inline V3 m4_project(M4 view_projection, V3 p) {
  const double *e = view_projection.e;
  double w = 1 / (e[3] * p.x + e[7] * p.y + e[11] * p.z + e[15]);
  return v3((e[0] * p.x + e[4] * p.y + e[8] * p.z + e[12]) * w,
            (e[1] * p.x + e[5] * p.y + e[9] * p.z + e[13]) * w,
            (e[2] * p.x + e[6] * p.y + e[10] * p.z + e[14]) * w);
}

#endif
