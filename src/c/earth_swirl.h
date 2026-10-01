#pragma once
#include <pebble.h>
#include "earth_math.h"

// ============================================================================
// ==================== DISCRETE ZOOM LUT CONFIGURATION =======================
// ============================================================================
// zoom_value: Matches settings values (100, 120, 140, 180)
// edge_pct:   Swirl band depth as % of radius (4%, 7%, 10%)
// max_deg:    Swirl rotation degrees (10, 20, 35)
// ring_px:    Inner loupe ring thickness (0px, 2px, 4px, 6px) - GROWS WITH ZOOM!

typedef struct {
  int32_t zoom_value;
  int32_t edge_pct;
  int32_t max_deg;
  int32_t ring_px;     // <-- Dynamic ring thickness per zoom level!
} SwirlZoomEntry;

static const SwirlZoomEntry SWIRL_ZOOM_LUT[] = {
  { 100,  0,   0, 0 },   // Whole Globe: No swirl, 0px loupe ring (clean sphere)
  { 120,  4,  10, 2 },   // Sub-Hemisphere: Subtle 4% swirl, 2px loupe ring
  { 140,  7,  20, 3 },   // Continental: 7% swirl, 4px loupe ring
  { 180, 10,  35, 5 }    // Regional (Max zoom): 10% swirl, 6px bold loupe ring
};

#define SWIRL_LUT_COUNT ((int)(sizeof(SWIRL_ZOOM_LUT) / sizeof(SwirlZoomEntry)))

// Swirl rotation direction: 1 for clockwise, -1 for counter-clockwise
#define SWIRL_DIRECTION             1

// ============================================================================
// ==================== BEZEL & SHADOW PROFILE SETTINGS =======================
// ============================================================================

// Outer Border (At the edge of the watch globe)
#define ENABLE_OUTER_BEZEL          1   // 1 = solid outer ring, 0 = disabled
#define OUTER_BEZEL_WIDTH_PX        1   // Outer ring thickness (px)

// Outer shadow on inside of outer bezel (kept to 1px so it doesn't crush the swirl)
#define ENABLE_OUTER_SHADOW         1   
#define OUTER_SHADOW_WIDTH_PX       1   

// Inner shadow placed on outside of loupe ring (framing the swirl)
#define ENABLE_INNER_SHADOW         1   
#define INNER_SHADOW_WIDTH_PX       2   

// ============================================================================

typedef struct {
  bool active;
  int32_t r_start;
  int32_t r_start2;
  int32_t band_width;
  int32_t max_deg;
  int32_t ring_px;
} SwirlParams;

static inline SwirlParams swirl_init_params(int32_t R, int32_t current_zoom) {
  SwirlParams p;
  memset(&p, 0, sizeof(SwirlParams));

  int32_t edge_pct = 0;
  int32_t max_deg = 0;
  int32_t ring_px = 0;

  for (int i = 0; i < SWIRL_LUT_COUNT; i++) {
    if (current_zoom <= SWIRL_ZOOM_LUT[i].zoom_value) {
      edge_pct = SWIRL_ZOOM_LUT[i].edge_pct;
      max_deg = SWIRL_ZOOM_LUT[i].max_deg;
      ring_px = SWIRL_ZOOM_LUT[i].ring_px;
      break;
    }
    if (i == SWIRL_LUT_COUNT - 1) {
      edge_pct = SWIRL_ZOOM_LUT[i].edge_pct;
      max_deg = SWIRL_ZOOM_LUT[i].max_deg;
      ring_px = SWIRL_ZOOM_LUT[i].ring_px;
    }
  }

  p.ring_px = ring_px;

  if (edge_pct > 0 && max_deg > 0) {
    p.active = true;
    p.r_start = (R * (100 - edge_pct)) / 100;
    p.r_start2 = p.r_start * p.r_start;
    p.band_width = R - p.r_start;
    if (p.band_width < 1) p.band_width = 1;
    p.max_deg = max_deg;
  } else {
    p.active = false;
    p.r_start = R;
    p.r_start2 = R * R;
    p.band_width = 1;
    p.max_deg = 0;
  }

  return p;
}

// Coordinate distortion: only warps pixels situated in the outer swirl band
static inline void swirl_transform_point(int32_t *x, int32_t *y, int32_t r2, const SwirlParams *p, bool invert) {
  if (!p->active || r2 <= p->r_start2) return;

  int32_t cur_r = earth_int_sqrt(r2);
  if (cur_r <= p->r_start) return;

  int32_t t = ((cur_r - p->r_start) * 256) / p->band_width;
  if (t > 256) t = 256;

  int32_t t2 = (t * t) / 256;
  int32_t delta_deg = (p->max_deg * t2) / 256;
  int32_t dir = invert ? -SWIRL_DIRECTION : SWIRL_DIRECTION;
  int32_t swirl_angle = ((int32_t)delta_deg * TRIG_MAX_ANGLE * dir) / 360;

  int32_t cos_val = cos_lookup(swirl_angle);
  int32_t sin_val = sin_lookup(swirl_angle);

  int32_t orig_x = *x;
  int32_t orig_y = *y;
  *x = (int32_t)(((int64_t)orig_x * cos_val - (int64_t)orig_y * sin_val) / TRIG_MAX_RATIO);
  *y = (int32_t)(((int64_t)orig_x * sin_val + (int64_t)orig_y * cos_val) / TRIG_MAX_RATIO);
}

// Draws the complete dynamic loupe overlay
static inline void swirl_draw_optical_overlay(GContext *ctx, int cx, int cy, int R, const SwirlParams *sp, GColor fg) {
  
  // -------------------------------------------------------------
  // 1. OUTER RIM (Perimeter of the globe)
  // -------------------------------------------------------------
#if ENABLE_OUTER_BEZEL
  graphics_context_set_stroke_color(ctx, fg);
  for (int i = 0; i < OUTER_BEZEL_WIDTH_PX; i++) {
    graphics_draw_circle(ctx, GPoint(cx, cy), R + i);
  }
#endif

  // If not zoomed in (Whole Globe), no inner loupe or shadows needed
  if (!sp->active || sp->ring_px <= 0) return;

#if ENABLE_OUTER_SHADOW
  // Thin dark shadow on the inner side of the outer bezel
  graphics_context_set_stroke_color(ctx, GColorBlack);
  for (int i = 1; i <= OUTER_SHADOW_WIDTH_PX; i++) {
    graphics_draw_circle(ctx, GPoint(cx, cy), R - i);
  }
#endif

  // -------------------------------------------------------------
  // 2. INNER LOUPE RIM (Dynamic thickness based on zoom level)
  // -------------------------------------------------------------
  int r_inner = sp->r_start;

#if ENABLE_INNER_SHADOW
  // Dark shadow between the swirl and the loupe collar
  graphics_context_set_stroke_color(ctx, GColorBlack);
  for (int i = 1; i <= INNER_SHADOW_WIDTH_PX; i++) {
    graphics_draw_circle(ctx, GPoint(cx, cy), r_inner + i);
  }
#endif

  // Dynamic solid foreground collar (grows thicker as zoom increases)
  graphics_context_set_stroke_color(ctx, fg);
  for (int i = 0; i < sp->ring_px; i++) {
    graphics_draw_circle(ctx, GPoint(cx, cy), r_inner - i);
  }
}