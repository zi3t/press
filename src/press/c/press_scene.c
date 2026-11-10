// The Press choreography, ported from src/press/runtime/clean-room/scene.ts
// (layout, the route/gesture callbacks and animate) and lighting.ts. The DOM
// halves of those functions stay in scene.ts, which calls these through the
// press_* exports and reads PressOutput.
#include <stdio.h>
#include <stdlib.h>

#include "press.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#define EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT
#endif

Press P;

static const RGB BACK_LIGHT_REST = {0x1a / 255.0f, 0x1a / 255.0f, 0x1a / 255.0f};

static bool compact(void) { return P.in.compact != 0; }
static bool narrow(void) { return P.in.narrow != 0; }
static bool reduced(void) { return P.in.reduced_motion != 0; }

// ── object matrices and projection ─────────────────────────────────────────

M4 book_world_matrix(const Book *b) {
  return m4_mul(m4_compose(b->root_position, b->root_rotation, b->root_scale),
                m4_compose(b->cover_position, b->cover_rotation, 1));
}

M4 camera_view_at(double camera_y) {
  M4 world = m4_compose(v3(0, camera_y, P.camera_z), (Euler){REF_CAMERA_PITCH, 0, 0, ORDER_XYZ}, 1);
  return m4_rigid_inverse(world);
}

static void update_camera(void) {
  P.view = camera_view_at(P.camera_y);
  P.projection = m4_perspective(P.fov, P.aspect, 1, 650);
  P.view_projection = m4_mul(P.projection, P.view);
}

// projectedBookBounds: the mesh's bounding-box corners through the camera.
Bounds book_bounds(const Book *b, M4 view_projection, double width, double height) {
  M4 world = book_world_matrix(b), m = m4_mul(view_projection, world);
  double left = INFINITY, top = INFINITY, right = -INFINITY, bottom = -INFINITY;
  for (int i = 0; i < 8; i += 1) {
    V3 corner = v3(i & 1 ? book_bounds_max[0] : book_bounds_min[0], i & 2 ? book_bounds_max[1] : book_bounds_min[1],
                   i & 4 ? book_bounds_max[2] : book_bounds_min[2]);
    V3 p = m4_project(m, corner);
    double sx = (p.x + 1) * width * 0.5, sy = (1 - p.y) * height * 0.5;
    left = fmin(left, sx); top = fmin(top, sy); right = fmax(right, sx); bottom = fmax(bottom, sy);
  }
  if (!isfinite(left)) return (Bounds){0};
  double r2 = 100;
  return (Bounds){round(left * r2) / r2, round(top * r2) / r2, round((right - left) * r2) / r2,
                  round((bottom - top) * r2) / r2, true};
}

static double world_units_per_pixel(double depth) {
  return 2 * tan(P.fov * 0.5 * M_PI / 180) * (P.camera_z - depth) / P.in.viewport_height;
}

// ── lighting.ts ────────────────────────────────────────────────────────────

static void lights_update(double camera_y, double presentation) {
  double active = clampd(presentation, 0, 1);
  P.rake_position = v3(24, camera_y, 1);
  P.rake_target = v3(mixd(-6, -14.3, active), camera_y - 6.5, mixd(-6.5, -61, active));
}

// ── poses ──────────────────────────────────────────────────────────────────

static void set_shelf_orders(Book *b) {
  b->root_rotation = euler_reorder(b->root_rotation, ORDER_XYZ);
  b->cover_rotation = euler_reorder(b->cover_rotation, ORDER_ZYX);
}

static void set_active_orders(Book *b) {
  b->root_rotation = euler_reorder(b->root_rotation, ORDER_XYZ);
  b->cover_rotation = euler_reorder(b->cover_rotation, ORDER_XYZ);
}

static void set_rotation(Euler *e, double x, double y, double z) { e->x = x; e->y = y; e->z = z; }

// Stripe's book shader is fully opaque. Transitions move or hide the mesh;
// fractional surface alpha would print rear faces through the cover.
static void set_book_opacity(Book *b, double opacity) {
  b->opacity = clampd(opacity, 0, 1);
  b->visible = b->opacity > 0.001;
}

static V3 active_position(void) {
  if (compact()) return v3(REF_COMPACT_ACTIVE_X, REF_COMPACT_ACTIVE_Y, REF_COMPACT_ACTIVE_Z);
  double canvas_width = fmin(REF_CANVAS_MAX_WIDTH, P.in.viewport_width);
  double ratio = canvas_width / REF_POSITION_REFERENCE_WIDTH;
  return v3(fmax(REF_ACTIVE_X, REF_ACTIVE_X * ratio), REF_ACTIVE_Y, REF_ACTIVE_Z);
}

static void snap_to_shelf(Book *b) {
  if (!b->initialized) return;
  set_shelf_orders(b);
  b->root_position = b->home_position;
  b->root_scale = b->home_scale;
  set_rotation(&b->root_rotation, REF_SHELF_ROOT_ROTATION_X, 0, REF_SHELF_ROOT_ROTATION_Z);
  b->cover_position = v3(REF_SHELF_COVER_OFFSET_X, 0, 0);
  set_rotation(&b->cover_rotation, 0, REF_COVER_BASE_ROTATION_Y, 0);
  b->section_weight = 0;
  b->section_visible = false;
  b->section_pose = 0;
  set_book_opacity(b, 1);
}

// transformResetProductList: every book is staged around the product that
// was active, then the list recurrence returns them to the shelf.
static void stage_reference_list_return(int active_index) {
  double shelf_gap = narrow() ? -7 : -6, list_y = REF_ACTIVE_Y;
  for (int i = 0; i < P.count; i += 1) {
    Book *b = &P.books[i];
    set_shelf_orders(b);
    b->root_position.y = list_y + (active_index - i) * 100;
    list_y += shelf_gap;
    b->hover = b->hold = 0;
    b->section_weight = 0;
    b->section_visible = false;
    b->section_pose = 0;
    set_book_opacity(b, 1);
  }
  P.poster_position.y = P.poster_target_y - 33;  // FILM_RESET_GAP
}

// ── exports: setup ─────────────────────────────────────────────────────────

EXPORT PressInput *press_input(void) { return &P.in; }
EXPORT PressOutput *press_output(void) { return &P.out; }

EXPORT void press_set_book(int i, double thickness, double specular, double shininess, double reflectiveness,
                           double diffuse_base, double bump_base, double bump_custom, int base_bump,
                           double foil_detail, double foil_opacity, double foil_specular, double foil_emissive,
                           double gloss_opacity, double gloss_specular, double gloss_emissive,
                           double glitter_opacity, double glitter_specular, double glitter_emissive,
                           double cloth_r, double cloth_g, double cloth_b) {
  if (i < 0 || i >= PRESS_MAX_BOOKS) return;
  if (i >= P.count) P.count = i + 1;
  Book *b = &P.books[i];
  b->thickness = thickness;
  b->specular = (RGB){(float)specular, (float)specular, (float)specular};
  b->shininess = shininess;
  b->reflectiveness = reflectiveness;
  b->diffuse_base = (RGB){(float)diffuse_base, (float)diffuse_base, (float)diffuse_base};
  b->bump_base = bump_base;
  b->bump_custom = bump_custom;
  b->base_bump = base_bump;
  b->foil_detail = foil_detail; b->foil_opacity = foil_opacity; b->foil_specular = foil_specular; b->foil_emissive = foil_emissive;
  b->gloss_opacity = gloss_opacity; b->gloss_specular = gloss_specular; b->gloss_emissive = gloss_emissive;
  b->glitter_opacity = glitter_opacity; b->glitter_specular = glitter_specular; b->glitter_emissive = glitter_emissive;
  b->cloth = (RGB){(float)cloth_r, (float)cloth_g, (float)cloth_b};
  b->root_rotation = (Euler){0, 0, 0, ORDER_XYZ};
  b->cover_rotation = (Euler){0, 0, 0, ORDER_XYZ};
  b->root_scale = 1;
  set_book_opacity(b, 1);
}

EXPORT void press_set_mode(int volumes, int current_route_index) {
  P.mode_volumes = volumes;
  P.current_route_index = current_route_index;
}

EXPORT int press_init(void) {
  P.camera_y = REF_CAMERA_Y;
  P.camera_z = REF_CAMERA_BASE_Z;
  P.fov = 12;
  P.aspect = 1;
  P.canvas_scale = 1;
  P.units_per_pixel = 0.024;
  P.back_color = BACK_LIGHT_REST;
  P.rake_intensity = 0.75 * LEGACY_LIGHT_SCALE;
  P.backdrop = BACK_LIGHT_REST;
  P.returning_index = P.returning_route_index = -1;
  P.entry_complete = P.in.reduced_motion != 0;
  P.poster_target_y = -200;
  P.poster_position = v3(0, -200, -200);
  P.poster_rotation = (Euler){0, 0, 0, ORDER_XYZ};
  P.poster_deformation = NAN;
  lights_update(P.camera_y, 0);
  if (!render_init() || !signal_init()) return 0;
  return 1;
}

// ── exports: route and gesture callbacks (scene.ts options) ────────────────

EXPORT void press_gesture_begin(int index) {
  P.returning_index = index;
  P.released_at = 0;
  P.released_from_drag = false;
  if (index >= 0 && index < P.count) P.backdrop = P.books[index].cloth;
}

EXPORT void press_gesture_finish(int index, int moved, double now) {
  P.returning_index = index;
  P.released_at = now;
  P.released_from_drag = moved;
}

EXPORT void press_before_volume(int index, int deep_link) {
  P.returning_index = -1;
  P.released_at = 0;
  P.released_from_drag = false;
  P.returning_route_index = -1;
  bool entering_from_catalogue = !P.mode_volumes;
  P.flight = (!entering_from_catalogue || index >= P.count || reduced() || compact() || deep_link)
    ? (Flight){0}
    : (Flight){true, index, true, 0, 0, 0};
}

// Returns whether the return is animated (scene.ts toggles
// press-catalogue-returning on that).
EXPORT int press_before_catalogue(int index) {
  P.returning_route_index = index;
  bool animate_return = !reduced() && !compact();
  if (animate_return) stage_reference_list_return(index);
  P.flight = animate_return ? (Flight){true, index, false, 0, 0, 0} : (Flight){0};
  if (!animate_return)
    for (int i = 0; i < P.count; i += 1) snap_to_shelf(&P.books[i]);
  return animate_return;
}

EXPORT void press_mode_change(int volumes) {
  P.mode_volumes = volumes;
  if (volumes) P.entry_complete = true;
}

EXPORT void press_index_change(int index) { P.current_route_index = index; }

// ── layout() ───────────────────────────────────────────────────────────────

static void place_camera_for_scroll(void) {
  double catalogue_maximum = P.in.current_scroll_step * fmax(0, P.count - 1);
  double catalogue_scroll_y = fmin(P.in.scroll_y, catalogue_maximum);
  double ratio = REF_CATALOGUE_CAMERA_SCROLL / (P.in.viewport_height / REF_CANVAS_REFERENCE_HEIGHT) * P.canvas_scale;
  P.camera_y = !P.mode_volumes ? REF_CAMERA_Y - catalogue_scroll_y * ratio : REF_CAMERA_Y;
  update_camera();
}

// The per-entry half of layout(). Returns 1 when it ran (the page then
// recalibrates its DOM), 0 when only the camera moved.
EXPORT int press_layout(void) {
  P.mode_volumes = P.in.mode_volumes != 0;
  P.current_route_index = (int)P.in.current_route_index;
  double width = P.in.viewport_width, height = P.in.viewport_height;
  bool viewport_changed = width != P.rendered_width || height != P.rendered_height;
  if (viewport_changed) {
    P.rendered_width = width;
    P.rendered_height = height;
    P.aspect = width / height;
    P.fov = narrow() ? 15 : 12;
    double canvas_width = fmin(REF_CANVAS_MAX_WIDTH, width);
    P.canvas_scale = narrow() ? 1
      : fmax(1, height * (REF_CANVAS_MAX_WIDTH / REF_CANVAS_REFERENCE_HEIGHT) / canvas_width * REF_CANVAS_OVERSCAN);
    P.camera_z = REF_CAMERA_BASE_Z * P.canvas_scale;
  }
  place_camera_for_scroll();
  lights_update(P.camera_y, P.mode_volumes ? 1 : 0);

  bool mode_changed = !P.laid_out || P.laid_out_mode_volumes != P.mode_volumes;
  P.laid_out = true;
  P.laid_out_mode_volumes = P.mode_volumes;
  bool needs_entry_layout = viewport_changed || mode_changed || P.mode_volumes;
  for (int i = 0; i < P.count; i += 1) needs_entry_layout |= !P.books[i].initialized;
  if (!needs_entry_layout) return 0;

  V3 section_target = active_position();
  double section_units_per_pixel = world_units_per_pixel(section_target.z);
  if (P.mode_volumes && !reduced()) {
    int incoming = -1;
    for (int i = 0; i < P.count; i += 1) {
      const double *r = P.in.figure[i];
      if (r[0] && r[2] > 0 && r[1] < height && r[1] + r[2] > -180) incoming = i;
    }
    P.presented_route_index = incoming >= 0 ? incoming : P.current_route_index;
  } else {
    P.presented_route_index = P.current_route_index;
  }

  double shelf_center_depth = REF_SHELF_ROOT_Z - 11;
  for (int i = 0; i < P.count; i += 1) {
    Book *b = &P.books[i];
    double shelf_gap = narrow() ? -7 : -6;
    b->home_position = v3(0, i * shelf_gap, REF_SHELF_ROOT_Z);
    b->home_scale = REF_MODEL_SCALE;

    const double *rect = P.in.figure[i];
    bool has_rect = P.mode_volumes && rect[0];
    bool section_eligible = P.mode_volumes && !reduced() && has_rect;
    if (has_rect) {
      double top = rect[1], rect_height = rect[2], bottom = top + rect_height;
      b->section_position = section_target;
      double figure_center = top + rect_height * 0.5;
      if (!compact()) b->section_position.y = section_target.y + (height * 0.5 - figure_center) * section_units_per_pixel;
      b->section_scale = REF_MODEL_SCALE;
      b->section_weight = section_eligible && i == P.presented_route_index ? 1 : 0;
      b->section_visible = b->section_weight > 0 && bottom > -180 && top < height + 180;
      b->section_pose = b->section_weight > 0 ? smooth01(clampd((height - figure_center) / (height * 0.5), 0, 1)) : 0;
      b->section_turn_y = b->section_weight > 0 && !compact()
        ? (height * 0.5 - figure_center) * MOTION_VOLUME_SCROLL_TURN * b->section_pose : 0;
    } else {
      b->section_position = b->home_position;
      b->section_scale = b->home_scale;
      b->section_weight = 0;
      b->section_visible = false;
      b->section_pose = 0;
      b->section_turn_y = 0;
    }
    if (!b->initialized) {
      if (b->section_weight > 0 && i == P.current_route_index) {
        set_active_orders(b);
        b->root_position = b->section_position;
        b->root_scale = b->section_scale;
        set_rotation(&b->root_rotation, REF_ACTIVE_ROTATION_X + (narrow() ? REF_NARROW_ACTIVE_PITCH_OFFSET : 0),
                     REF_ACTIVE_ROTATION_Y + b->section_turn_y, REF_ACTIVE_ROTATION_Z);
        b->cover_position = v3(0, 0, 0);
        set_rotation(&b->cover_rotation, 0, REF_COVER_BASE_ROTATION_Y, 0);
      } else {
        set_shelf_orders(b);
        b->root_position = v3(b->home_position.x,
                              reduced() ? b->home_position.y : MOTION_ENTRY_INITIAL_Y - i * MOTION_ENTRY_INITIAL_GAP,
                              reduced() ? b->home_position.z
                                        : MOTION_ENTRY_INITIAL_Z - sin((double)i / P.count) * MOTION_ENTRY_INITIAL_DEPTH_ARC);
        b->root_scale = b->home_scale;
        set_rotation(&b->root_rotation, REF_SHELF_ROOT_ROTATION_X, 0, REF_SHELF_ROOT_ROTATION_Z);
        b->cover_position = v3(REF_SHELF_COVER_OFFSET_X, 0, 0);
        set_rotation(&b->cover_rotation, 0, REF_COVER_BASE_ROTATION_Y, 0);
      }
      b->initialized = true;
    }
  }
  P.units_per_pixel = world_units_per_pixel(shelf_center_depth);

  // foldoutPoster.setLayout
  double film_book_gap = narrow() ? -23 : compact() ? -34 : -19;
  if (compact() || reduced() || P.count == 0) {
    P.poster_layout_ready = false;
    P.poster_visible = false;
  } else {
    bool first = P.poster_target_y == -200;
    P.poster_layout_ready = true;
    P.poster_target_y = P.books[P.count - 1].home_position.y + film_book_gap;
    P.poster_small_screen = compact();
    if (first) P.poster_position.y = P.poster_target_y;
  }
  P.out.presented_route_index = P.presented_route_index;
  P.out.units_per_pixel = P.units_per_pixel;
  return 1;
}

// handleScroll in catalogue mode: the camera follows the shelf.
EXPORT void press_scroll(void) {
  P.mode_volumes = P.in.mode_volumes != 0;
  place_camera_for_scroll();
  lights_update(P.camera_y, 0);
}

// ── animate() ──────────────────────────────────────────────────────────────

static void advance_poster(bool isolating, int active_index, const Flight *flight) {
  // Exact updateFilmPosters scalar from Stripe's current Canvas bundle.
  double deformation = clampd((P.in.film_source_document_top + P.in.viewport_height * 0.5 - P.in.scroll_y) * 0.0017, 0.08, 1.4);
  poster_set_deformation(deformation);
  double terminal_travel_world = P.in.terminal_travel * P.units_per_pixel;
  double selection_return_offset = active_index >= 0 ? -33 * P.hold_isolation : 0;
  bool returning = flight->active && !flight->to_volume;
  double next_y = P.poster_target_y + terminal_travel_world + selection_return_offset;
  P.poster_position.y = returning ? mixd(P.poster_position.y, next_y, flight->approach) : next_y;
  P.poster_position.z = (P.poster_small_screen ? -60 : -18) - (1 - P.poster_deformation) * 50;
  double tilt = 0.3;  // FILM_TILT_LIMIT
  P.poster_rotation = (Euler){P.in.poster_pointer_y / fmax(1, P.in.viewport_height) * tilt,
                              P.in.poster_pointer_x / fmax(1, P.in.viewport_width) * tilt, P.poster_deformation / 25, ORDER_XYZ};
  bool frame_visible = !P.mode_volumes && !compact() && !reduced() && !isolating;
  P.poster_visible = P.poster_ready && P.poster_layout_ready && frame_visible;
  P.poster_residual = returning ? fabs(P.poster_position.y - next_y) : 0;
}

static double residual_max(double a, double b) { return a > b ? a : b; }

static void animate_volume_book(Book *b, int i, const Flight *flight) {
  if (b->section_weight <= 0) { set_book_opacity(b, 0); return; }
  bool presented = i == P.presented_route_index;
  bool interactive = presented && i == P.current_route_index;
  double rx = presented ? (REF_ACTIVE_ROTATION_X + (narrow() ? REF_NARROW_ACTIVE_PITCH_OFFSET : 0)) * b->section_pose : 0;
  double ry = presented ? REF_ACTIVE_ROTATION_Y * b->section_pose + b->section_turn_y : 0;
  double rz = presented ? REF_ACTIVE_ROTATION_Z * b->section_pose : 0;
  double crx = interactive ? P.in.volume_rotation_x : 0;
  double cry = REF_COVER_BASE_ROTATION_Y + (interactive ? P.in.volume_rotation_y : 0);
  set_active_orders(b);
  if (flight->active && flight->to_volume && flight->index == i) {
    double a = flight->approach;
    b->root_position = v3_lerp(b->root_position, b->section_position, a);
    b->root_scale = mixd(b->root_scale, b->section_scale, a);
    b->root_rotation.x = mixd(b->root_rotation.x, rx, a);
    b->root_rotation.y = mixd(b->root_rotation.y, ry, a);
    b->root_rotation.z = mixd(b->root_rotation.z, rz, a);
    b->cover_position.x = mixd(b->cover_position.x, 0, a);
    b->cover_rotation.x = mixd(b->cover_rotation.x, crx, a);
    b->cover_rotation.y = mixd(b->cover_rotation.y, cry, a);
    b->cover_rotation.z = mixd(b->cover_rotation.z, 0, a);
    double residual = v3_distance(b->root_position, b->section_position);
    residual = residual_max(residual, fabs(b->root_scale - b->section_scale) * 20);
    residual = residual_max(residual, fabs(b->root_rotation.x - rx) * 10);
    residual = residual_max(residual, fabs(b->root_rotation.y - ry) * 10);
    residual = residual_max(residual, fabs(b->root_rotation.z - rz) * 10);
    residual = residual_max(residual, fabs(b->cover_position.x) * 10);
    residual = residual_max(residual, fabs(b->cover_rotation.x - crx) * 10);
    residual = residual_max(residual, fabs(b->cover_rotation.y - cry) * 10);
    P.entry_residual = residual_max(P.entry_residual, residual);
    if (residual < 0.012) P.flight.active = false;
  } else {
    b->root_position = b->section_position;
    b->root_scale = b->section_scale;
    set_rotation(&b->root_rotation, rx, ry, rz);
    b->cover_position = v3(0, 0, 0);
    set_rotation(&b->cover_rotation, crx, cry, 0);
  }
  set_book_opacity(b, b->section_visible ? 1 : 0);
}

static void animate_returning_book(Book *b, double a) {
  double shelf_y = b->home_position.y;
  set_shelf_orders(b);
  b->root_position.x = mixd(b->root_position.x, b->home_position.x, a);
  b->root_position.y = mixd(b->root_position.y, shelf_y, a);
  b->root_position.z = mixd(b->root_position.z, b->home_position.z, a);
  b->root_scale = mixd(b->root_scale, b->home_scale, a);
  b->root_rotation.x = mixd(b->root_rotation.x, REF_SHELF_ROOT_ROTATION_X, a);
  b->root_rotation.y = mixd(b->root_rotation.y, 0, a);
  b->root_rotation.z = mixd(b->root_rotation.z, REF_SHELF_ROOT_ROTATION_Z, a);
  b->cover_position.x = mixd(b->cover_position.x, REF_SHELF_COVER_OFFSET_X, a);
  b->cover_rotation.x = mixd(b->cover_rotation.x, 0, a);
  b->cover_rotation.y = mixd(b->cover_rotation.y, REF_COVER_BASE_ROTATION_Y, a);
  b->cover_rotation.z = mixd(b->cover_rotation.z, 0, a);
  set_book_opacity(b, 1);
  double r = fabs(b->root_position.x - b->home_position.x);
  r = residual_max(r, fabs(b->root_position.y - shelf_y));
  r = residual_max(r, fabs(b->root_position.z - b->home_position.z));
  r = residual_max(r, fabs(b->root_scale - b->home_scale) * 20);
  r = residual_max(r, fabs(b->root_rotation.x - REF_SHELF_ROOT_ROTATION_X) * 10);
  r = residual_max(r, fabs(b->root_rotation.y) * 10);
  r = residual_max(r, fabs(b->root_rotation.z - REF_SHELF_ROOT_ROTATION_Z) * 10);
  r = residual_max(r, fabs(b->cover_position.x - REF_SHELF_COVER_OFFSET_X) * 10);
  r = residual_max(r, fabs(b->cover_rotation.x) * 10);
  r = residual_max(r, fabs(b->cover_rotation.y - REF_COVER_BASE_ROTATION_Y) * 10);
  r = residual_max(r, fabs(b->opacity - 1) * 10);
  P.entry_residual = residual_max(P.entry_residual, r);
}

static void animate_shelf_book(Book *b, int i, int active_index, bool returning_presentation, double entry_approach) {
  double dt = P.in.dt;
  bool gesture = P.in.gesture_index >= 0;
  bool actively_held = gesture && (int)P.in.gesture_index == i;
  bool interactive = !reduced() && ((int)P.in.hover_index == i || (int)P.in.focus_index == i || actively_held);
  bool hold_target = actively_held || (i == P.returning_index && returning_presentation);
  set_shelf_orders(b);
  b->hover = damp(b->hover, interactive ? 1 : 0, interactive ? 9.2 : 6.8, dt);
  b->hold = damp(b->hold, hold_target ? 1 : 0, hold_target ? 15 : 6.5, dt);

  double eviction_pixels = i == active_index ? 0
    : (active_index - i) * P.in.viewport_height * MOTION_STACK_EVICTION_VIEWPORTS * P.hold_isolation;
  double target_y = b->home_position.y + b->hold * MOTION_HELD_LIFT_PIXELS * P.units_per_pixel
    + P.in.terminal_travel * P.units_per_pixel + eviction_pixels * P.units_per_pixel;
  double target_depth = b->home_position.z + (REF_SHELF_HOVER_Z - b->home_position.z) * fmax(b->hover, b->hold);
  bool terminal_tracking = P.in.terminal_progress > 0;
  bool settling = P.entry_complete;  // shelfApproach === null

  b->root_position.x = settling ? damp(b->root_position.x, b->home_position.x, 11.5, dt) : mixd(b->root_position.x, b->home_position.x, entry_approach);
  b->root_position.y = terminal_tracking ? target_y
    : settling ? damp(b->root_position.y, target_y, 11.5, dt) : mixd(b->root_position.y, target_y, entry_approach);
  b->root_position.z = settling ? frame_approach(b->root_position.z, target_depth, MOTION_SPINE_Z_EASE, dt)
                                : mixd(b->root_position.z, target_depth, entry_approach);
  b->root_scale = settling ? damp(b->root_scale, b->home_scale, 11, dt) : mixd(b->root_scale, b->home_scale, entry_approach);
  b->cover_position.x = damp(b->cover_position.x, REF_SHELF_COVER_OFFSET_X, 13, dt);
  b->cover_position.y = 0;
  b->cover_position.z = 0;
  b->root_rotation.x = damp(b->root_rotation.x, REF_SHELF_ROOT_ROTATION_X + P.in.scroll_velocity * (1 - b->hold), 13, dt);
  b->root_rotation.y = damp(b->root_rotation.y, 0, 13, dt);
  b->root_rotation.z = damp(b->root_rotation.z, REF_SHELF_ROOT_ROTATION_Z, 13, dt);
  if (actively_held) {
    b->cover_rotation.x = wrap_rotation(P.in.gesture_dx * MOTION_VOLUME_DRAG_RATE);
    b->cover_rotation.y = wrap_rotation(REF_COVER_BASE_ROTATION_Y - P.in.gesture_dy * MOTION_VOLUME_DRAG_RATE);
    b->cover_rotation.z = 0;
  } else {
    b->cover_rotation.x = damp(b->cover_rotation.x, 0, 7.5, dt);
    b->cover_rotation.y = damp(b->cover_rotation.y, REF_COVER_BASE_ROTATION_Y, 7.5, dt);
    b->cover_rotation.z = damp(b->cover_rotation.z, 0, 7.5, dt);
  }
  set_book_opacity(b, P.in.terminal_scene_opacity);
  double r = fabs(b->root_position.y - target_y);
  r = residual_max(r, fabs(b->root_position.z - target_depth));
  r = residual_max(r, fabs(b->root_scale - b->home_scale));
  P.entry_residual = residual_max(P.entry_residual, r);
}

// One frame of animate(): everything but the DOM writes, which the page
// derives from PressOutput, and the render, which press_render() does.
EXPORT void press_frame(void) {
  double now = P.in.now, dt = P.in.dt;
  int events = 0;
  P.mode_volumes = P.in.mode_volumes != 0;
  P.current_route_index = (int)P.in.current_route_index;

  bool gesture = P.in.gesture_index >= 0;
  int active_index = gesture ? (int)P.in.gesture_index : P.returning_index;
  bool holding = gesture, presenting = holding;
  double release_elapsed = P.released_at ? now - P.released_at : INFINITY;
  bool isolating = holding || (P.released_from_drag && release_elapsed < MOTION_RELEASE_ISOLATION);
  bool returning_presentation = P.released_from_drag && release_elapsed < MOTION_RELEASE_PRESENTATION;
  bool backing_presentation = presenting || (P.released_from_drag && release_elapsed < MOTION_RELEASE_BACKDROP);
  P.hold_isolation = damp(P.hold_isolation, isolating ? 1 : 0, isolating ? 14 : 6, dt);
  P.hold_presentation = damp(P.hold_presentation, presenting || returning_presentation ? 1 : 0,
                             presenting || returning_presentation ? 7.8 : 4.6, dt);
  P.hold_backdrop = damp(P.hold_backdrop, backing_presentation ? 1 : 0, backing_presentation ? 12.5 : 7, dt);
  P.out.stack_evacuated = active_index >= 0 && P.hold_isolation > 0.9;

  Flight *flight = &P.flight;
  bool matches_mode = flight->active && ((flight->to_volume && P.mode_volumes) || (!flight->to_volume && !P.mode_volumes));
  if (flight->active && matches_mode) {
    double frames = clampd(dt * 60, 0.25, 4);
    flight->speed = fmin(MOTION_FLIGHT_EASE_CEILING, flight->speed + MOTION_FLIGHT_EASE_STEP * frames);
    flight->approach = 1 - pow(1 - flight->speed, frames);
    flight->progress = 1 - (1 - flight->progress) * pow(1 - flight->speed, frames);
  }
  // scene.ts keeps its own reference (activeFlight) even when it drops a
  // flight that no longer matches the mode, so copy before dropping.
  Flight active_flight = *flight;
  if (flight->active && !matches_mode) flight->active = false;

  bool poster_selection_returning = active_index >= 0
    && (P.hold_isolation > 0.002 || P.hold_presentation > 0.002 || P.hold_backdrop > 0.002);
  advance_poster(isolating, active_index, &active_flight);

  P.out.signal_animating = signal_advance(dt);

  double entry_frames = clampd(dt * 60, 0.25, 4);
  if (!P.entry_complete && !P.mode_volumes)
    P.entry_speed = fmin(MOTION_FLIGHT_EASE_CEILING, P.entry_speed + MOTION_FLIGHT_EASE_STEP * entry_frames);
  double entry_approach = P.entry_complete || reduced() ? 1 : 1 - pow(1 - P.entry_speed, entry_frames);
  P.entry_residual = 0;
  for (int i = 0; i < P.count; i += 1) {
    Book *b = &P.books[i];
    if (P.mode_volumes) animate_volume_book(b, i, &active_flight);
    else if (active_flight.active && !active_flight.to_volume) animate_returning_book(b, active_flight.approach);
    else animate_shelf_book(b, i, active_index, returning_presentation, entry_approach);
  }

  if (active_flight.active && !active_flight.to_volume && P.entry_residual <= MOTION_ENTRY_SETTLE_EPSILON && P.flight.active) {
    // Measure only the settled shelf: snap the same books home, then let the
    // page drop press-catalogue-returning and relayout.
    for (int i = 0; i < P.count; i += 1) snap_to_shelf(&P.books[i]);
    P.flight.active = false;
    P.returning_route_index = -1;
    events |= PRESS_EVENT_RETURN_SETTLED;
  }
  if (!P.mode_volumes && P.returning_route_index >= 0 && !P.flight.active && P.entry_residual <= MOTION_ENTRY_SETTLE_EPSILON) {
    P.returning_route_index = -1;
    events |= PRESS_EVENT_RETURN_CLEARED;
  }
  if (!P.entry_complete && P.entry_residual <= MOTION_ENTRY_SETTLE_EPSILON) {
    P.entry_complete = true;
    events |= PRESS_EVENT_ENTRY_COMPLETE;
  }
  // How far the fly-in has come, so the page can pace the rail to land with
  // the books. The residual decays exponentially toward the settle epsilon,
  // so its logarithm advances evenly with time and reaches 1 exactly at the
  // frame that completes the entry.
  if (P.entry_complete) {
    P.out.entry_progress = 1;
  } else {
    if (P.entry_residual_start <= 0) P.entry_residual_start = P.entry_residual;
    double eps = MOTION_ENTRY_SETTLE_EPSILON, start = P.entry_residual_start, now_r = P.entry_residual;
    P.out.entry_progress = start <= eps || now_r <= eps ? 1 : clampd(1 - log(now_r / eps) / log(start / eps), 0, 1);
  }

  // Lights: a held shelf book keeps the shelf rake, not the detail target.
  double route_presentation = P.mode_volumes ? 1 : P.hold_presentation;
  lights_update(P.camera_y, P.mode_volumes ? 1 : 0);
  int active_entry = P.mode_volumes ? P.current_route_index : active_index;
  RGB back_target = BACK_LIGHT_REST;
  if (active_entry >= 0 && active_entry < P.count && route_presentation > 0.001) {
    RGB cloth = P.books[active_entry].cloth;
    double t = 1 - route_presentation;
    back_target = (RGB){(float)mixd(cloth.r, BACK_LIGHT_REST.r, t), (float)mixd(cloth.g, BACK_LIGHT_REST.g, t),
                        (float)mixd(cloth.b, BACK_LIGHT_REST.b, t)};
  }
  double k = 1 - exp(-5.2 * dt);
  P.back_color = (RGB){(float)mixd(P.back_color.r, back_target.r, k), (float)mixd(P.back_color.g, back_target.g, k),
                       (float)mixd(P.back_color.b, back_target.b, k)};
  P.rake_intensity = damp(P.rake_intensity, (P.mode_volumes ? 0.05 : 0.75) * LEGACY_LIGHT_SCALE, 5.2, dt);

  P.out.active_index = active_entry < P.count ? active_entry : -1;
  P.out.holding_presentation = !P.mode_volumes && active_entry >= 0 && P.hold_presentation > 0.001;
  if (!holding && P.hold_isolation < 0.002 && P.hold_presentation < 0.002 && P.hold_backdrop < 0.002) {
    P.returning_index = -1;
    P.released_at = 0;
    P.released_from_drag = false;
    P.out.stack_evacuated = 0;
  }

  P.out.events = events;
  P.out.flight_active = P.flight.active;
  P.out.entry_complete = P.entry_complete;
  P.out.backdrop_r = P.backdrop.r; P.out.backdrop_g = P.backdrop.g; P.out.backdrop_b = P.backdrop.b;
  P.out.backdrop_alpha = P.hold_backdrop;
  P.out.poster_selection_returning = poster_selection_returning;
  Bounds pb = poster_bounds();
  P.out.poster_visible = pb.valid;
  P.out.poster_left = pb.left; P.out.poster_top = pb.top; P.out.poster_width = pb.width; P.out.poster_height = pb.height;
  P.out.idle_ready = !P.out.signal_animating && P.entry_complete && !gesture && P.returning_index < 0
    && P.returning_route_index < 0 && !P.flight.active && P.in.scroll_velocity == 0
    && P.entry_residual <= MOTION_ENTRY_SETTLE_EPSILON && P.poster_residual <= 0.0005;
  P.out.units_per_pixel = P.units_per_pixel;
}

// ── exports: projection for the page's hit targets ─────────────────────────

static double bounds_out[4];

static double *write_bounds(Bounds b) {
  if (!b.valid) return NULL;
  bounds_out[0] = b.left; bounds_out[1] = b.top; bounds_out[2] = b.width; bounds_out[3] = b.height;
  return bounds_out;
}

// The book's projected screen box in CSS pixels, or NULL.
EXPORT double *press_book_bounds(int i) {
  if (i < 0 || i >= P.count) return NULL;
  return write_bounds(book_bounds(&P.books[i], P.view_projection, P.in.viewport_width, P.in.viewport_height));
}

// The same through a camera at another height (terminal calibration).
EXPORT double *press_book_bounds_at(int i, double camera_y) {
  if (i < 0 || i >= P.count) return NULL;
  M4 vp = m4_mul(P.projection, camera_view_at(camera_y));
  return write_bounds(book_bounds(&P.books[i], vp, P.in.viewport_width, P.in.viewport_height));
}

// Camera height for a catalogue scroll position (projectedTerminalBookBounds).
EXPORT double press_camera_y_for_scroll(double scroll_y) {
  double ratio = REF_CATALOGUE_CAMERA_SCROLL / (P.in.viewport_height / REF_CANVAS_REFERENCE_HEIGHT) * P.canvas_scale;
  return REF_CAMERA_Y - scroll_y * ratio;
}

EXPORT void press_render(void) { render_frame(); }
