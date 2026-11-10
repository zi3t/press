// Shared state for the Press scene: the C/raylib port of
// src/press/runtime/clean-room/{scene,geometry,material,lighting,
// foldout-poster,signal}.ts. The page (scene.ts) owns the DOM, routing,
// scroll measurement and pointer capture; this owns every 3D object, its
// motion, the camera and lights, projection, and drawing.
#ifndef PRESS_H
#define PRESS_H

#include <stdbool.h>

#include "raylib.h"

#include "press_math.h"

#define PRESS_MAX_BOOKS 8

// CLEAN_ROOM_REFERENCE
#define REF_CANVAS_MAX_WIDTH 2000.0
#define REF_CANVAS_REFERENCE_HEIGHT 1018.0
#define REF_CANVAS_OVERSCAN 1.1
#define REF_POSITION_REFERENCE_WIDTH 1792.0
#define REF_CAMERA_BASE_Z 100.0
#define REF_CAMERA_Y 6.5
#define REF_CAMERA_PITCH (-0.06)
#define REF_CATALOGUE_CAMERA_SCROLL 0.0222
#define REF_MODEL_SCALE 1.0
#define REF_SHELF_ROOT_Z (-3.0)
#define REF_SHELF_HOVER_Z 1.5
#define REF_SHELF_COVER_OFFSET_X 11.0
#define REF_SHELF_ROOT_ROTATION_X (-(M_PI / 2))
#define REF_SHELF_ROOT_ROTATION_Z (M_PI / 2)
#define REF_COVER_BASE_ROTATION_Y (-(M_PI / 2))
#define REF_ACTIVE_X (-13.0)
#define REF_ACTIVE_Y (-4.0)
#define REF_ACTIVE_Z (-56.0)
#define REF_ACTIVE_ROTATION_X (-0.5)
#define REF_ACTIVE_ROTATION_Y 0.35
#define REF_ACTIVE_ROTATION_Z 0.15
#define REF_COMPACT_ACTIVE_X 0.0
#define REF_COMPACT_ACTIVE_Y 3.0
#define REF_COMPACT_ACTIVE_Z (-90.0)
#define REF_NARROW_ACTIVE_PITCH_OFFSET 0.16

// CLEAN_ROOM_MOTION
#define MOTION_ENTRY_SETTLE_EPSILON 0.012
#define MOTION_ENTRY_INITIAL_Y 3.0
#define MOTION_ENTRY_INITIAL_GAP 3.0
#define MOTION_ENTRY_INITIAL_Z (-50.0)
#define MOTION_ENTRY_INITIAL_DEPTH_ARC 150.0
#define MOTION_STACK_EVICTION_VIEWPORTS 1.12
#define MOTION_SPINE_Z_EASE 0.1
#define MOTION_RELEASE_ISOLATION 360.0
#define MOTION_RELEASE_PRESENTATION 110.0
#define MOTION_RELEASE_BACKDROP 430.0
#define MOTION_FLIGHT_EASE_STEP 0.006
#define MOTION_FLIGHT_EASE_CEILING 0.15
#define MOTION_VOLUME_DRAG_RATE 0.003
#define MOTION_VOLUME_SCROLL_TURN 0.0008
#define MOTION_HELD_LIFT_PIXELS 0.0
#define LEGACY_LIGHT_SCALE M_PI

typedef struct { float r, g, b; } RGB;

typedef struct {
  // Material profile (profiles.ts) and per-book appearance.
  double thickness;
  RGB specular, diffuse_base, cloth;
  double shininess, reflectiveness, bump_base, bump_custom;
  double foil_detail, foil_opacity, foil_specular, foil_emissive;
  double gloss_opacity, gloss_specular, gloss_emissive;
  double glitter_opacity, glitter_specular, glitter_emissive;
  int base_bump;  // 0 none, 1 buckram, 2 paper, 3 cardboard

  // The book's root group and its child cover mesh, as in geometry.ts.
  V3 root_position, cover_position;
  Euler root_rotation, cover_rotation;
  double root_scale;
  bool visible;

  // CleanRoomEntry
  V3 home_position, section_position;
  double home_scale, section_scale, section_pose, section_turn_y, section_weight;
  bool section_visible, initialized;
  double opacity, hover, hold;
} Book;

typedef struct {
  bool active;
  int index;
  bool to_volume;  // else to the catalogue
  double speed, approach, progress;
} Flight;

typedef struct { double left, top, width, height; bool valid; } Bounds;

// What the page writes before press_layout / press_frame. Field order is the
// layout of a Float64Array on the page (PRESS_INPUT in scene.ts).
typedef struct {
  double now, dt;
  double viewport_width, viewport_height, device_pixel_ratio;
  double compact, narrow, reduced_motion;
  double mode_volumes, current_route_index;
  double scroll_y, current_scroll_step;
  // catalogue scroll snapshot
  double terminal_travel, terminal_progress, scroll_velocity, terminal_scene_opacity;
  // interaction snapshot
  double hover_index, focus_index, gesture_index, gesture_dx, gesture_dy;
  // volume pose
  double volume_rotation_x, volume_rotation_y;
  // layout: figure rects of each volume, [valid, top, height] per book
  double figure[PRESS_MAX_BOOKS][3];
  // foldout poster
  double film_source_document_top, poster_pointer_x, poster_pointer_y;
  // signal section
  double signal_top, signal_bottom, signal_detail, signal_figure_valid;
  double signal_figure_left, signal_figure_top, signal_figure_width, signal_figure_height;
  double signal_pointer_x, signal_pointer_y;
} PressInput;

// What press_frame reports back (PRESS_OUTPUT in scene.ts).
typedef struct {
  double presented_route_index;
  double events;  // PRESS_EVENT_* bits raised this frame
  double flight_active, entry_complete, holding_presentation;
  double stack_evacuated, signal_animating, idle_ready;
  double backdrop_r, backdrop_g, backdrop_b, backdrop_alpha;
  double poster_visible, poster_left, poster_top, poster_width, poster_height;
  double active_index, poster_selection_returning;
  double units_per_pixel;
  double entry_progress;  // 0..1 through the first fly-in, 1 once settled
} PressOutput;

enum {
  PRESS_EVENT_ENTRY_COMPLETE = 1,   // books settled after the first entry
  PRESS_EVENT_RETURN_SETTLED = 2,   // a flight back to the shelf finished
  PRESS_EVENT_RETURN_CLEARED = 4    // returning route index cleared
};

typedef struct {
  int count;
  Book books[PRESS_MAX_BOOKS];
  PressInput in;
  PressOutput out;

  // camera
  double camera_y, camera_z, fov, aspect, canvas_scale;
  M4 view, projection, view_projection;
  double units_per_pixel;

  // lights (lighting.ts), world space
  RGB back_color;
  double rake_intensity;
  V3 rake_position, rake_target;

  // choreography
  bool mode_volumes, entry_complete, laid_out, laid_out_mode_volumes;
  double rendered_width, rendered_height, entry_speed;
  int current_route_index, presented_route_index, returning_index, returning_route_index;
  double released_at;
  bool released_from_drag;
  double hold_isolation, hold_presentation, hold_backdrop;
  RGB backdrop;
  Flight flight;
  double entry_residual, entry_residual_start;

  // foldout poster
  bool poster_layout_ready, poster_small_screen, poster_ready, poster_visible;
  double poster_target_y, poster_deformation, poster_residual;
  V3 poster_position;
  Euler poster_rotation;
} Press;

extern Press P;

// press_scene.c
M4 book_world_matrix(const Book *b);
Bounds book_bounds(const Book *b, M4 view_projection, double width, double height);
M4 camera_view_at(double camera_y);

// press_render.c
bool render_init(void);
void render_upload_texture(int id, const unsigned char *rgba, int width, int height);
void render_frame(void);
void poster_set_deformation(double value);
Bounds poster_bounds(void);
void render_set_book_mesh_bounds(double min[3], double max[3]);
extern double book_bounds_min[3], book_bounds_max[3];

// press_book.c
Mesh book_generate_mesh(void);
void book_generate_maps(void);

// press_signal.c
bool signal_init(void);
bool signal_advance(double dt);
void signal_render(void);

// texture ids shared with the page
enum {
  TEX_SHARED_OVERLAY = 0,
  TEX_BUMP_NONE = 1, TEX_BUMP_BUCKRAM = 2, TEX_BUMP_PAPER = 3, TEX_BUMP_CARDBOARD = 4,
  TEX_GLITTER = 5,
  TEX_POSTER = 6,
  TEX_DISC_0 = 7, TEX_DISC_1 = 8, TEX_DISC_2 = 9,
  TEX_BOOK_BASE = 16  // + book * 4 + {0 diffuse, 1 bump, 2 foil, 3 gloss}
};
#define TEX_COUNT (TEX_BOOK_BASE + PRESS_MAX_BOOKS * 4)

#endif
