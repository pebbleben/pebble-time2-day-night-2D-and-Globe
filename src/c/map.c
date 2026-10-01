#include <pebble.h>
#include "map.h"
#include "layout.h"
#include "settings.h"
#include "earth_math.h"
#include "earth_map.h"
#include "earth_swirl.h"

#define TERMINATOR_LINE 1
#define MAX_MAP_WIDTH 512

#define MAP_WIDTH_3D 512
#define MAP_HEIGHT_3D 256

static Layer *s_canvas;
static GBitmap *s_world_bitmap = NULL;
#ifdef PBL_COLOR
static GBitmap *s_night_bitmap = NULL;
#endif
static int s_day_index = -1, s_night_index = -1;
static int s_time_offset = 0;
static uint8_t s_previous_row[MAX_MAP_WIDTH];

static inline uint8_t get_terrain_3d(int u, int v) {
  u = u & (MAP_WIDTH_3D - 1);
  if (v < 0) v = 0;
  if (v >= MAP_HEIGHT_3D) v = MAP_HEIGHT_3D - 1;

  int byte_index = v * (MAP_WIDTH_3D / 4) + (u >> 2);
  uint8_t byte = s_earth_map[byte_index];
  int shift = (u & 0x03) * 2;
  return (byte >> shift) & 0x03;
}

static uint8_t read_pixel_2d(GBitmap *bmp, int x, int y) {
  const uint8_t *row = gbitmap_get_data(bmp) + y * gbitmap_get_bytes_per_row(bmp);
  switch (gbitmap_get_format(bmp)) {
    case GBitmapFormat1Bit:
      return (row[x >> 3] & (1u << (x & 7))) ? 0xFF : 0xC0;
#ifdef PBL_COLOR
    case GBitmapFormat8Bit: return row[x];
    case GBitmapFormat1BitPalette:
      return gbitmap_get_palette(bmp)[(row[x >> 3] >> (7-(x&7))) & 1].argb;
    case GBitmapFormat2BitPalette:
      return gbitmap_get_palette(bmp)[(row[x >> 2] >> (6-2*(x&3))) & 3].argb;
    case GBitmapFormat4BitPalette:
      return gbitmap_get_palette(bmp)[(row[x >> 1] >> (4*(1-(x&1)))) & 15].argb;
#endif
    default: return 0xC0;
  }
}

static uint8_t sample_2d(GBitmap *bmp, int32_t u_ratio, int32_t v_ratio) {
  GSize size = gbitmap_get_bounds(bmp).size;
  int x = (int)((int64_t)u_ratio * size.w / FP_SCALE);
  int y = (int)((int64_t)v_ratio * size.h / FP_SCALE);
  if (x < 0) x = 0;
  if (x >= size.w) x = size.w - 1;
  if (y < 0) y = 0;
  if (y >= size.h) y = size.h - 1;
  return read_pixel_2d(bmp, x, y);
}

static GColor pixel_color_2d(ClaySettings *s, int32_t u_ratio, int32_t v_ratio, bool night) {
#ifdef PBL_COLOR
  GBitmap *bmp = night ? s_night_bitmap : s_world_bitmap;
  int index = night ? s->NightMapIndex : s->DayMapIndex;
  if (index == 2 || !bmp) {
    int u = (int)((int64_t)u_ratio * MAP_WIDTH_3D / FP_SCALE);
    int v = (int)((int64_t)v_ratio * MAP_HEIGHT_3D / FP_SCALE);
    uint8_t terrain = get_terrain_3d(u, v);
    if (terrain == 1) return night ? s->NightLand : s->DayLand;
    if (terrain == 2) return night ? s->NightIce : s->DayIce;
    return night ? s->NightWater : s->DayWater;
  }
  uint8_t raw = sample_2d(bmp, u_ratio, v_ratio);
  return (GColor){.argb = raw | 0xC0};
#else
  bool white = s_world_bitmap ? (sample_2d(s_world_bitmap, u_ratio, v_ratio) == 0xFF) : true;
  return (white != night) ? GColorWhite : GColorBlack;
#endif
}

static void draw_marker(GContext *ctx, GPoint pt, GColor color) {
  GColor border_color = (color.argb == GColorBlack.argb || 
                         color.argb == GColorOxfordBlue.argb ||
                         color.argb == GColorDarkGreen.argb) ? GColorWhite : GColorBlack;

  graphics_context_set_fill_color(ctx, border_color);
  graphics_fill_circle(ctx, pt, 4);

  graphics_context_set_fill_color(ctx, color);
  graphics_fill_circle(ctx, pt, 3);
}

static void draw_watch(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  ClaySettings *s = settings_get();

  int32_t sun_lat_deg = 0;
  int32_t sun_lon_deg = 0;
  EarthVector sun = earth_sun(time(NULL) + s_time_offset, &sun_lat_deg, &sun_lon_deg);

  int32_t view_lon = s->LongitudeOffset;
  int16_t view_lat = (int16_t)s->LatitudeOffset;

//   if (s->CenterFocus == 1) {
//     view_lon = sun_lon_deg;
//   } else if (s->CenterFocus == 2) {
//     view_lon = earth_wrap_deg(sun_lon_deg + 180);
//   }
  if (s->CenterFocus == 1) {
    // Center directly on the sun (day side)
    view_lon = sun_lon_deg;
  } else if (s->CenterFocus == 2) {
    // Center on the anti-sun point (night side)
    view_lon = earth_wrap_deg(sun_lon_deg + 180);
  } else if (s->CenterFocus == 3) {
    // Daybreak: the terminator where night turns into day (sunrise line).
    // The sunrise terminator leads the sun by 90 degrees.
    view_lon = earth_wrap_deg(sun_lon_deg - 90);
  } else if (s->CenterFocus == 4) {
    // Nightfall: the terminator where day turns into night (sunset line).
    // The sunset terminator trails the sun by 90 degrees.
    view_lon = earth_wrap_deg(sun_lon_deg + 90);
  }
  
  bool is_3d = (s->MapProjection == MAP_PROJECTION_3D);

  if (is_3d) {
    // ======================== 3D GLOBE RENDERER ========================
    int d = MAP_3D_GLOBE_DIAMETER_PX;
    int R = d / 2;
    int R2 = R * R;
    
    int cx = (bounds.size.w / 2) + MAP_3D_NUDGE_X_PX;
    int cy = g_dynamic_map_center_y + MAP_3D_NUDGE_Y_PX;

    int32_t zoom = s->AltitudeZoom;
    if (zoom < 100) zoom = 100;

    // Initialize discrete LUT swirl parameters for current zoom
    SwirlParams sp = swirl_init_params(R, zoom);

    EarthCamera camera = earth_camera(view_lat, (int16_t)view_lon);
    memset(s_previous_row, 2, sizeof(s_previous_row));

    for (int y = -R; y <= R; y++) {
      int screen_y = cy + y;
      int32_t y2 = (int32_t)y * y;
      if (y2 > R2) continue;

      int32_t h_radius = earth_int_sqrt(R2 - y2);
      int left_pixel_night = 2;

      for (int x = -h_radius; x <= h_radius; x++) {
        int screen_x = cx + x;
        int col_x = x + R;

        int32_t r2 = (int32_t)x * x + y2;
        if (r2 > R2) {
          s_previous_row[col_x] = 2;
          left_pixel_night = 2;
          continue;
        }

        int32_t sample_x = x;
        int32_t sample_y = -y;

        // Apply swirl distortion to sample coordinates
        swirl_transform_point(&sample_x, &sample_y, r2, &sp, false);

        int32_t sx_unit = ((int64_t)sample_x * FP_SCALE * 100) / (R * zoom);
        int32_t sy_unit = ((int64_t)sample_y * FP_SCALE * 100) / (R * zoom);

        int32_t r2_unit = ((int64_t)sx_unit * sx_unit + (int64_t)sy_unit * sy_unit) / FP_SCALE;
        if (r2_unit > FP_SCALE) {
          s_previous_row[col_x] = 2;
          left_pixel_night = 2;
          continue;
        }

        int32_t sz_unit = earth_int_sqrt(FP_SCALE * FP_SCALE - ((int64_t)sx_unit * sx_unit + (int64_t)sy_unit * sy_unit));

        EarthVector p;
        p.x = (sx_unit * camera.right.x + sy_unit * camera.up.x + sz_unit * camera.forward.x) / FP_SCALE;
        p.y = (sx_unit * camera.right.y + sy_unit * camera.up.y + sz_unit * camera.forward.y) / FP_SCALE;
        p.z = (sx_unit * camera.right.z + sy_unit * camera.up.z + sz_unit * camera.forward.z) / FP_SCALE;

        int32_t horiz = earth_int_sqrt(p.x * p.x + p.z * p.z);
        int32_t lat_angle = (int16_t)atan2_lookup((int16_t)p.y, (int16_t)horiz);
        
        int32_t v_numer = (int32_t)lat_angle * MAP_HEIGHT_3D;
        int32_t v_denom = TRIG_MAX_ANGLE / 2;
        int v_map = (MAP_HEIGHT_3D / 2) - ((v_numer + (v_numer >= 0 ? v_denom / 2 : -v_denom / 2)) / v_denom);
        if (v_map < 0) v_map = 0;
        if (v_map >= MAP_HEIGHT_3D) v_map = MAP_HEIGHT_3D - 1;

        int32_t lon_angle = atan2_lookup((int16_t)p.z, (int16_t)p.x) + (TRIG_MAX_ANGLE / 2);
        int32_t u_numer = (int64_t)lon_angle * MAP_WIDTH_3D;
        int u_map = (u_numer + (TRIG_MAX_ANGLE / 2)) / TRIG_MAX_ANGLE;
        u_map = u_map & (MAP_WIDTH_3D - 1);

        uint8_t terrain = get_terrain_3d(u_map, v_map);

        bool is_night = (earth_dot(p, sun) < 0);
        bool is_edge = (left_pixel_night != 2 && left_pixel_night != is_night) ||
                       (s_previous_row[col_x] != 2 && s_previous_row[col_x] != is_night);

        GColor pixel_color;
        if (is_edge) {
          pixel_color = GColorWhite;
        } else {
          if (terrain == 1) pixel_color = is_night ? s->NightLand : s->DayLand;
          else if (terrain == 2) pixel_color = is_night ? s->NightIce : s->DayIce;
          else pixel_color = is_night ? s->NightWater : s->DayWater;
        }

        graphics_context_set_stroke_color(ctx, pixel_color);
        graphics_draw_pixel(ctx, GPoint(screen_x, screen_y));

        s_previous_row[col_x] = is_night ? 1 : 0;
        left_pixel_night = is_night ? 1 : 0;
      }
    }

    // 3D Location Marker
    if (s->MarkerEnabled) {
      int32_t m_lat_angle = ((int32_t)s->MarkerLat * TRIG_MAX_ANGLE) / 360;
      int32_t m_lon_angle = ((int32_t)s->MarkerLon * TRIG_MAX_ANGLE) / 360;
      int32_t m_cl = (int32_t)cos_lookup(m_lat_angle) / 4;
      int32_t m_sl = (int32_t)sin_lookup(m_lat_angle) / 4;
      int32_t m_so = (int32_t)sin_lookup(m_lon_angle) / 4;
      int32_t m_co = (int32_t)cos_lookup(m_lon_angle) / 4;

      EarthVector m_p;
      m_p.x = (m_cl * m_co) / FP_SCALE;
      m_p.y = m_sl;
      m_p.z = (m_cl * m_so) / FP_SCALE;

      if (earth_dot(m_p, camera.forward) > 0) {
        int32_t m_sx = earth_dot(m_p, camera.right);
        int32_t m_sy = earth_dot(m_p, camera.up);

        int32_t dot_rel_x = (int32_t)(((int64_t)m_sx * R * zoom) / ((int64_t)FP_SCALE * FP_SCALE * 100));
        int32_t dot_rel_y = -(int32_t)(((int64_t)m_sy * R * zoom) / ((int64_t)FP_SCALE * FP_SCALE * 100));

        int32_t m_r2 = dot_rel_x * dot_rel_x + dot_rel_y * dot_rel_y;
        if (m_r2 <= R2) {
          swirl_transform_point(&dot_rel_x, &dot_rel_y, m_r2, &sp, true);
          draw_marker(ctx, GPoint(cx + dot_rel_x, cy + dot_rel_y), s->MarkerColor);
        }
      }
    }

    // Draw the complete optical viewfinder overlay (dashed ring, shadow, ticks, and glare)
//     swirl_draw_optical_overlay(ctx, cx, cy, R, &sp, s->ForegroundColor, s->BackgroundColor);
    swirl_draw_optical_overlay(ctx, cx, cy, R, &sp, s->ForegroundColor);

  } else {
    // ======================== 2D MAP RENDERER ========================
    int w = MAP_2D_PROJECTION_WIDTH_PX;
    int h = MAP_2D_PROJECTION_HEIGHT_PX;
    
    int start_x = ((bounds.size.w - w) / 2) + MAP_2D_NUDGE_X_PX;
    int start_y = (g_dynamic_map_center_y - (h / 2)) + MAP_2D_NUDGE_Y_PX;

    memset(s_previous_row, 2, sizeof(s_previous_row));

    for (int y = 0; y < h; ++y) {
      int left_pixel_night = 2;
      int32_t lat_deg = 90 - (y * 180) / h;
      int32_t lat_angle = (lat_deg * TRIG_MAX_ANGLE) / 360;
      int32_t cl = (int32_t)cos_lookup(lat_angle) / 4;
      int32_t sl = (int32_t)sin_lookup(lat_angle) / 4;

      int32_t v_ratio = (y * FP_SCALE) / h;

      for (int x = 0; x < w; ++x) {
        int32_t lon_deg = earth_wrap_deg(view_lon + ((x * 360) / w - 180));
        int32_t lon_angle = (lon_deg * TRIG_MAX_ANGLE) / 360;
        int32_t so = (int32_t)sin_lookup(lon_angle) / 4;
        int32_t co = (int32_t)cos_lookup(lon_angle) / 4;

        EarthVector p;
        p.x = (cl * co) / FP_SCALE;
        p.y = sl;
        p.z = (cl * so) / FP_SCALE;

        int32_t u_ratio = (((lon_deg + 180) * FP_SCALE) / 360) & (FP_SCALE - 1);

        bool is_night = (earth_dot(p, sun) < 0);
        bool is_edge = (left_pixel_night != 2 && left_pixel_night != is_night) ||
                       (s_previous_row[x] != 2 && s_previous_row[x] != is_night);

        GColor pixel_color;
        if (is_edge) {
          pixel_color = GColorWhite;
        } else {
          pixel_color = pixel_color_2d(s, u_ratio, v_ratio, is_night);
        }

        graphics_context_set_stroke_color(ctx, pixel_color);
        graphics_draw_pixel(ctx, GPoint(start_x + x, start_y + y));

        s_previous_row[x] = is_night ? 1 : 0;
        left_pixel_night = is_night ? 1 : 0;
      }
    }

    if (s->MarkerEnabled) {
      int32_t rel_lon = earth_wrap_deg(s->MarkerLon - view_lon);
      int dot_x = start_x + ((rel_lon + 180) * w) / 360;
      int dot_y = start_y + ((90 - s->MarkerLat) * h) / 180;

      if (dot_x >= start_x && dot_x < start_x + w &&
          dot_y >= start_y && dot_y < start_y + h) {
        draw_marker(ctx, GPoint(dot_x, dot_y), s->MarkerColor);
      }
    }
  }
}

void map_init(Layer *parent_layer, GRect bounds) {
  s_canvas = layer_create(bounds);
  if (!s_canvas) return;
  layer_set_update_proc(s_canvas, draw_watch);
  layer_add_child(parent_layer, s_canvas);
}

void map_deinit(void) {
  if (s_canvas) layer_destroy(s_canvas);
  s_canvas = NULL;
  if (s_world_bitmap) gbitmap_destroy(s_world_bitmap);
  s_world_bitmap = NULL;
#ifdef PBL_COLOR
  if (s_night_bitmap) gbitmap_destroy(s_night_bitmap);
  s_night_bitmap = NULL;
#endif
  s_day_index = s_night_index = -1;
}

void map_set_time_offset(int offset) { s_time_offset = offset; }
void map_force_redraw(void) { if (s_canvas) layer_mark_dirty(s_canvas); }

void map_reload_bitmaps(int day_index, int night_index) {
  ClaySettings *s = settings_get();

  if (s->MapProjection == MAP_PROJECTION_3D) {
    if (s_world_bitmap) { gbitmap_destroy(s_world_bitmap); s_world_bitmap = NULL; }
#ifdef PBL_COLOR
    if (s_night_bitmap) { gbitmap_destroy(s_night_bitmap); s_night_bitmap = NULL; }
#endif
    s_day_index = s_night_index = -1;
    return;
  }

#ifdef PBL_COLOR
  if (day_index != s_day_index || !s_world_bitmap) {
    if (s_world_bitmap) gbitmap_destroy(s_world_bitmap);
    uint32_t id = RESOURCE_ID_DAY_01_CHARLIE;
    if (day_index == 1) id = RESOURCE_ID_DAY_02_BLUE_MARBLE;
    if (day_index == 2) id = RESOURCE_ID_3_Color_Map;
    s_world_bitmap = gbitmap_create_with_resource(id);
    s_day_index = day_index;
  }
  if (night_index != s_night_index || !s_night_bitmap) {
    if (s_night_bitmap) gbitmap_destroy(s_night_bitmap);
    uint32_t id = RESOURCE_ID_NIGHT_01_DITHER;
    if (night_index == 1) id = RESOURCE_ID_NIGHT_02_CLEAN;
    if (night_index == 2) id = RESOURCE_ID_3_Color_Map;
    s_night_bitmap = gbitmap_create_with_resource(id);
    s_night_index = night_index;
  }
#else
  (void)day_index; (void)night_index;
  if (!s_world_bitmap) s_world_bitmap = gbitmap_create_with_resource(RESOURCE_ID_WORLD);
#endif
}



// #include <pebble.h>
// #include "map.h"
// #include "layout.h"
// #include "settings.h"
// #include "earth_math.h"
// #include "earth_map.h"

// #define TERMINATOR_LINE 1
// #define MAX_MAP_WIDTH 512

// #define MAP_WIDTH_3D 512
// #define MAP_HEIGHT_3D 256

// static Layer *s_canvas;
// static GBitmap *s_world_bitmap = NULL;
// #ifdef PBL_COLOR
// static GBitmap *s_night_bitmap = NULL;
// #endif
// static int s_day_index = -1, s_night_index = -1;
// static int s_time_offset = 0;
// static uint8_t s_previous_row[MAX_MAP_WIDTH];

// static inline uint8_t get_terrain_3d(int u, int v) {
//   u = u & (MAP_WIDTH_3D - 1);
//   if (v < 0) v = 0;
//   if (v >= MAP_HEIGHT_3D) v = MAP_HEIGHT_3D - 1;

//   int byte_index = v * (MAP_WIDTH_3D / 4) + (u >> 2);
//   uint8_t byte = s_earth_map[byte_index];
//   int shift = (u & 0x03) * 2;
//   return (byte >> shift) & 0x03;
// }

// static uint8_t read_pixel_2d(GBitmap *bmp, int x, int y) {
//   const uint8_t *row = gbitmap_get_data(bmp) + y * gbitmap_get_bytes_per_row(bmp);
//   switch (gbitmap_get_format(bmp)) {
//     case GBitmapFormat1Bit:
//       return (row[x >> 3] & (1u << (x & 7))) ? 0xFF : 0xC0;
// #ifdef PBL_COLOR
//     case GBitmapFormat8Bit: return row[x];
//     case GBitmapFormat1BitPalette:
//       return gbitmap_get_palette(bmp)[(row[x >> 3] >> (7-(x&7))) & 1].argb;
//     case GBitmapFormat2BitPalette:
//       return gbitmap_get_palette(bmp)[(row[x >> 2] >> (6-2*(x&3))) & 3].argb;
//     case GBitmapFormat4BitPalette:
//       return gbitmap_get_palette(bmp)[(row[x >> 1] >> (4*(1-(x&1)))) & 15].argb;
// #endif
//     default: return 0xC0;
//   }
// }

// static uint8_t sample_2d(GBitmap *bmp, int32_t u_ratio, int32_t v_ratio) {
//   GSize size = gbitmap_get_bounds(bmp).size;
//   int x = (int)((int64_t)u_ratio * size.w / FP_SCALE);
//   int y = (int)((int64_t)v_ratio * size.h / FP_SCALE);
//   if (x < 0) x = 0;
//   if (x >= size.w) x = size.w - 1;
//   if (y < 0) y = 0;
//   if (y >= size.h) y = size.h - 1;
//   return read_pixel_2d(bmp, x, y);
// }

// static GColor pixel_color_2d(ClaySettings *s, int32_t u_ratio, int32_t v_ratio, bool night) {
// #ifdef PBL_COLOR
//   GBitmap *bmp = night ? s_night_bitmap : s_world_bitmap;
//   int index = night ? s->NightMapIndex : s->DayMapIndex;
//   if (index == 2 || !bmp) {
//     int u = (int)((int64_t)u_ratio * MAP_WIDTH_3D / FP_SCALE);
//     int v = (int)((int64_t)v_ratio * MAP_HEIGHT_3D / FP_SCALE);
//     uint8_t terrain = get_terrain_3d(u, v);
//     if (terrain == 1) return night ? s->NightLand : s->DayLand;
//     if (terrain == 2) return night ? s->NightIce : s->DayIce;
//     return night ? s->NightWater : s->DayWater;
//   }
//   uint8_t raw = sample_2d(bmp, u_ratio, v_ratio);
//   return (GColor){.argb = raw | 0xC0};
// #else
//   bool white = s_world_bitmap ? (sample_2d(s_world_bitmap, u_ratio, v_ratio) == 0xFF) : true;
//   return (white != night) ? GColorWhite : GColorBlack;
// #endif
// }

// static void draw_marker(GContext *ctx, GPoint pt, GColor color) {
//   GColor border_color = (color.argb == GColorBlack.argb || 
//                          color.argb == GColorOxfordBlue.argb ||
//                          color.argb == GColorDarkGreen.argb) ? GColorWhite : GColorBlack;

//   graphics_context_set_fill_color(ctx, border_color);
//   graphics_fill_circle(ctx, pt, 4);

//   graphics_context_set_fill_color(ctx, color);
//   graphics_fill_circle(ctx, pt, 3);
// }

// static void draw_watch(Layer *layer, GContext *ctx) {
//   GRect bounds = layer_get_bounds(layer);
//   ClaySettings *s = settings_get();

//   int32_t sun_lat_deg = 0;
//   int32_t sun_lon_deg = 0;
//   EarthVector sun = earth_sun(time(NULL) + s_time_offset, &sun_lat_deg, &sun_lon_deg);

//   int32_t view_lon = s->LongitudeOffset;
//   int16_t view_lat = (int16_t)s->LatitudeOffset;

//   if (s->CenterFocus == 1) {
//     view_lon = sun_lon_deg;
//   } else if (s->CenterFocus == 2) {
//     view_lon = earth_wrap_deg(sun_lon_deg + 180);
//   }

//   bool is_3d = (s->MapProjection == MAP_PROJECTION_3D);

//   if (is_3d) {
//     // ======================== 3D GLOBE RENDERER ========================
//     int d = MAP_3D_GLOBE_DIAMETER_PX;
//     int R = d / 2;
//     int R2 = R * R;
    
//     // Position center with layout.h nudges
//     int cx = (bounds.size.w / 2) + MAP_3D_NUDGE_X_PX;
//     int cy = g_dynamic_map_center_y + MAP_3D_NUDGE_Y_PX;

//     int32_t zoom = s->AltitudeZoom;
//     if (zoom < 100) zoom = 100;

//     EarthCamera camera = earth_camera(view_lat, (int16_t)view_lon);
//     memset(s_previous_row, 2, sizeof(s_previous_row));

//     for (int y = -R; y <= R; y++) {
//       int screen_y = cy + y;
//       int32_t y2 = (int32_t)y * y;
//       if (y2 > R2) continue;

//       int32_t h_radius = earth_int_sqrt(R2 - y2);
//       int left_pixel_night = 2;

//       for (int x = -h_radius; x <= h_radius; x++) {
//         int screen_x = cx + x;
//         int col_x = x + R;

//         int32_t r2 = (int32_t)x * x + y2;
//         if (r2 > R2) {
//           s_previous_row[col_x] = 2;
//           left_pixel_night = 2;
//           continue;
//         }

//         int32_t sx_unit = ((int64_t)x * FP_SCALE * 100) / (R * zoom);
//         int32_t sy_unit = ((int64_t)(-y) * FP_SCALE * 100) / (R * zoom);

//         int32_t r2_unit = ((int64_t)sx_unit * sx_unit + (int64_t)sy_unit * sy_unit) / FP_SCALE;
//         if (r2_unit > FP_SCALE) {
//           s_previous_row[col_x] = 2;
//           left_pixel_night = 2;
//           continue;
//         }

//         int32_t sz_unit = earth_int_sqrt(FP_SCALE * FP_SCALE - ((int64_t)sx_unit * sx_unit + (int64_t)sy_unit * sy_unit));

//         EarthVector p;
//         p.x = (sx_unit * camera.right.x + sy_unit * camera.up.x + sz_unit * camera.forward.x) / FP_SCALE;
//         p.y = (sx_unit * camera.right.y + sy_unit * camera.up.y + sz_unit * camera.forward.y) / FP_SCALE;
//         p.z = (sx_unit * camera.right.z + sy_unit * camera.up.z + sz_unit * camera.forward.z) / FP_SCALE;

//         int32_t horiz = earth_int_sqrt(p.x * p.x + p.z * p.z);
//         int32_t lat_angle = (int16_t)atan2_lookup((int16_t)p.y, (int16_t)horiz);
        
//         int32_t v_numer = (int32_t)lat_angle * MAP_HEIGHT_3D;
//         int32_t v_denom = TRIG_MAX_ANGLE / 2;
//         int v_map = (MAP_HEIGHT_3D / 2) - ((v_numer + (v_numer >= 0 ? v_denom / 2 : -v_denom / 2)) / v_denom);
//         if (v_map < 0) v_map = 0;
//         if (v_map >= MAP_HEIGHT_3D) v_map = MAP_HEIGHT_3D - 1;

//         int32_t lon_angle = atan2_lookup((int16_t)p.z, (int16_t)p.x) + (TRIG_MAX_ANGLE / 2);
//         int32_t u_numer = (int64_t)lon_angle * MAP_WIDTH_3D;
//         int u_map = (u_numer + (TRIG_MAX_ANGLE / 2)) / TRIG_MAX_ANGLE;
//         u_map = u_map & (MAP_WIDTH_3D - 1);

//         uint8_t terrain = get_terrain_3d(u_map, v_map);

//         bool is_night = (earth_dot(p, sun) < 0);
//         bool is_edge = (left_pixel_night != 2 && left_pixel_night != is_night) ||
//                        (s_previous_row[col_x] != 2 && s_previous_row[col_x] != is_night);

//         GColor pixel_color;
//         if (is_edge) {
//           pixel_color = GColorWhite;
//         } else {
//           if (terrain == 1) pixel_color = is_night ? s->NightLand : s->DayLand;
//           else if (terrain == 2) pixel_color = is_night ? s->NightIce : s->DayIce;
//           else pixel_color = is_night ? s->NightWater : s->DayWater;
//         }

//         graphics_context_set_stroke_color(ctx, pixel_color);
//         graphics_draw_pixel(ctx, GPoint(screen_x, screen_y));

//         s_previous_row[col_x] = is_night ? 1 : 0;
//         left_pixel_night = is_night ? 1 : 0;
//       }
//     }

//     // 3D Location Marker
//     if (s->MarkerEnabled) {
//       int32_t m_lat_angle = ((int32_t)s->MarkerLat * TRIG_MAX_ANGLE) / 360;
//       int32_t m_lon_angle = ((int32_t)s->MarkerLon * TRIG_MAX_ANGLE) / 360;
//       int32_t m_cl = (int32_t)cos_lookup(m_lat_angle) / 4;
//       int32_t m_sl = (int32_t)sin_lookup(m_lat_angle) / 4;
//       int32_t m_so = (int32_t)sin_lookup(m_lon_angle) / 4;
//       int32_t m_co = (int32_t)cos_lookup(m_lon_angle) / 4;

//       EarthVector m_p;
//       m_p.x = (m_cl * m_co) / FP_SCALE;
//       m_p.y = m_sl;
//       m_p.z = (m_cl * m_so) / FP_SCALE;

//       if (earth_dot(m_p, camera.forward) > 0) {
//         int32_t m_sx = earth_dot(m_p, camera.right);
//         int32_t m_sy = earth_dot(m_p, camera.up);

//         int dot_x = cx + (int)(((int64_t)m_sx * R * zoom) / ((int64_t)FP_SCALE * FP_SCALE * 100));
//         int dot_y = cy - (int)(((int64_t)m_sy * R * zoom) / ((int64_t)FP_SCALE * FP_SCALE * 100));

//         int r_dist2 = (dot_x - cx) * (dot_x - cx) + (dot_y - cy) * (dot_y - cy);
//         if (r_dist2 <= R2) {
//           draw_marker(ctx, GPoint(dot_x, dot_y), s->MarkerColor);
//         }
//       }
//     }

//   } else {
//     // ======================== 2D MAP RENDERER ========================
//     int w = MAP_2D_PROJECTION_WIDTH_PX;
//     int h = MAP_2D_PROJECTION_HEIGHT_PX;
    
//     // Position top-left with layout.h nudges
//     int start_x = ((bounds.size.w - w) / 2) + MAP_2D_NUDGE_X_PX;
//     int start_y = (g_dynamic_map_center_y - (h / 2)) + MAP_2D_NUDGE_Y_PX;

//     memset(s_previous_row, 2, sizeof(s_previous_row));

//     for (int y = 0; y < h; ++y) {
//       int left_pixel_night = 2;
//       int32_t lat_deg = 90 - (y * 180) / h;
//       int32_t lat_angle = (lat_deg * TRIG_MAX_ANGLE) / 360;
//       int32_t cl = (int32_t)cos_lookup(lat_angle) / 4;
//       int32_t sl = (int32_t)sin_lookup(lat_angle) / 4;

//       int32_t v_ratio = (y * FP_SCALE) / h;

//       for (int x = 0; x < w; ++x) {
//         int32_t lon_deg = earth_wrap_deg(view_lon + ((x * 360) / w - 180));
//         int32_t lon_angle = (lon_deg * TRIG_MAX_ANGLE) / 360;
//         int32_t so = (int32_t)sin_lookup(lon_angle) / 4;
//         int32_t co = (int32_t)cos_lookup(lon_angle) / 4;

//         EarthVector p;
//         p.x = (cl * co) / FP_SCALE;
//         p.y = sl;
//         p.z = (cl * so) / FP_SCALE;

//         int32_t u_ratio = (((lon_deg + 180) * FP_SCALE) / 360) & (FP_SCALE - 1);

//         bool is_night = (earth_dot(p, sun) < 0);
//         bool is_edge = (left_pixel_night != 2 && left_pixel_night != is_night) ||
//                        (s_previous_row[x] != 2 && s_previous_row[x] != is_night);

//         GColor pixel_color;
//         if (is_edge) {
//           pixel_color = GColorWhite;
//         } else {
//           pixel_color = pixel_color_2d(s, u_ratio, v_ratio, is_night);
//         }

//         graphics_context_set_stroke_color(ctx, pixel_color);
//         graphics_draw_pixel(ctx, GPoint(start_x + x, start_y + y));

//         s_previous_row[x] = is_night ? 1 : 0;
//         left_pixel_night = is_night ? 1 : 0;
//       }
//     }

//     if (s->MarkerEnabled) {
//       int32_t rel_lon = earth_wrap_deg(s->MarkerLon - view_lon);
//       int dot_x = start_x + ((rel_lon + 180) * w) / 360;
//       int dot_y = start_y + ((90 - s->MarkerLat) * h) / 180;

//       if (dot_x >= start_x && dot_x < start_x + w &&
//           dot_y >= start_y && dot_y < start_y + h) {
//         draw_marker(ctx, GPoint(dot_x, dot_y), s->MarkerColor);
//       }
//     }
//   }
// }

// void map_init(Layer *parent_layer, GRect bounds) {
//   s_canvas = layer_create(bounds);
//   if (!s_canvas) return;
//   layer_set_update_proc(s_canvas, draw_watch);
//   layer_add_child(parent_layer, s_canvas);
// }

// void map_deinit(void) {
//   if (s_canvas) layer_destroy(s_canvas);
//   s_canvas = NULL;
//   if (s_world_bitmap) gbitmap_destroy(s_world_bitmap);
//   s_world_bitmap = NULL;
// #ifdef PBL_COLOR
//   if (s_night_bitmap) gbitmap_destroy(s_night_bitmap);
//   s_night_bitmap = NULL;
// #endif
//   s_day_index = s_night_index = -1;
// }

// void map_set_time_offset(int offset) { s_time_offset = offset; }
// void map_force_redraw(void) { if (s_canvas) layer_mark_dirty(s_canvas); }

// void map_reload_bitmaps(int day_index, int night_index) {
//   ClaySettings *s = settings_get();

//   if (s->MapProjection == MAP_PROJECTION_3D) {
//     if (s_world_bitmap) { gbitmap_destroy(s_world_bitmap); s_world_bitmap = NULL; }
// #ifdef PBL_COLOR
//     if (s_night_bitmap) { gbitmap_destroy(s_night_bitmap); s_night_bitmap = NULL; }
// #endif
//     s_day_index = s_night_index = -1;
//     return;
//   }

// #ifdef PBL_COLOR
//   if (day_index != s_day_index || !s_world_bitmap) {
//     if (s_world_bitmap) gbitmap_destroy(s_world_bitmap);
//     uint32_t id = RESOURCE_ID_DAY_01_CHARLIE;
//     if (day_index == 1) id = RESOURCE_ID_DAY_02_BLUE_MARBLE;
//     if (day_index == 2) id = RESOURCE_ID_3_Color_Map;
//     s_world_bitmap = gbitmap_create_with_resource(id);
//     s_day_index = day_index;
//   }
//   if (night_index != s_night_index || !s_night_bitmap) {
//     if (s_night_bitmap) gbitmap_destroy(s_night_bitmap);
//     uint32_t id = RESOURCE_ID_NIGHT_01_DITHER;
//     if (night_index == 1) id = RESOURCE_ID_NIGHT_02_CLEAN;
//     if (night_index == 2) id = RESOURCE_ID_3_Color_Map;
//     s_night_bitmap = gbitmap_create_with_resource(id);
//     s_night_index = night_index;
//   }
// #else
//   (void)day_index; (void)night_index;
//   if (!s_world_bitmap) s_world_bitmap = gbitmap_create_with_resource(RESOURCE_ID_WORLD);
// #endif
// }
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// #include <pebble.h>
// #include "map.h"
// #include "layout.h"
// #include "settings.h"
// #include "earth_math.h"
// #include "earth_map.h"

// #define TERMINATOR_LINE 1
// #define MAX_MAP_WIDTH 512

// #define MAP_WIDTH_3D 512
// #define MAP_HEIGHT_3D 256

// static Layer *s_canvas;
// static GBitmap *s_world_bitmap = NULL;
// #ifdef PBL_COLOR
// static GBitmap *s_night_bitmap = NULL;
// #endif
// static int s_day_index = -1, s_night_index = -1;
// static int s_time_offset = 0;
// static uint8_t s_previous_row[MAX_MAP_WIDTH];

// static inline uint8_t get_terrain_3d(int u, int v) {
//   u = u & (MAP_WIDTH_3D - 1);
//   if (v < 0) v = 0;
//   if (v >= MAP_HEIGHT_3D) v = MAP_HEIGHT_3D - 1;

//   int byte_index = v * (MAP_WIDTH_3D / 4) + (u >> 2);
//   uint8_t byte = s_earth_map[byte_index];
//   int shift = (u & 0x03) * 2;
//   return (byte >> shift) & 0x03;
// }

// static uint8_t read_pixel_2d(GBitmap *bmp, int x, int y) {
//   const uint8_t *row = gbitmap_get_data(bmp) + y * gbitmap_get_bytes_per_row(bmp);
//   switch (gbitmap_get_format(bmp)) {
//     case GBitmapFormat1Bit:
//       return (row[x >> 3] & (1u << (x & 7))) ? 0xFF : 0xC0;
// #ifdef PBL_COLOR
//     case GBitmapFormat8Bit: return row[x];
//     case GBitmapFormat1BitPalette:
//       return gbitmap_get_palette(bmp)[(row[x >> 3] >> (7-(x&7))) & 1].argb;
//     case GBitmapFormat2BitPalette:
//       return gbitmap_get_palette(bmp)[(row[x >> 2] >> (6-2*(x&3))) & 3].argb;
//     case GBitmapFormat4BitPalette:
//       return gbitmap_get_palette(bmp)[(row[x >> 1] >> (4*(1-(x&1)))) & 15].argb;
// #endif
//     default: return 0xC0;
//   }
// }

// static uint8_t sample_2d(GBitmap *bmp, int32_t u_ratio, int32_t v_ratio) {
//   GSize size = gbitmap_get_bounds(bmp).size;
//   int x = (int)((int64_t)u_ratio * size.w / FP_SCALE);
//   int y = (int)((int64_t)v_ratio * size.h / FP_SCALE);
//   if (x < 0) x = 0;
//   if (x >= size.w) x = size.w - 1;
//   if (y < 0) y = 0;
//   if (y >= size.h) y = size.h - 1;
//   return read_pixel_2d(bmp, x, y);
// }

// static GColor pixel_color_2d(ClaySettings *s, int32_t u_ratio, int32_t v_ratio, bool night) {
// #ifdef PBL_COLOR
//   GBitmap *bmp = night ? s_night_bitmap : s_world_bitmap;
//   int index = night ? s->NightMapIndex : s->DayMapIndex;
//   if (index == 2 || !bmp) {
//     int u = (int)((int64_t)u_ratio * MAP_WIDTH_3D / FP_SCALE);
//     int v = (int)((int64_t)v_ratio * MAP_HEIGHT_3D / FP_SCALE);
//     uint8_t terrain = get_terrain_3d(u, v);
//     if (terrain == 1) return night ? s->NightLand : s->DayLand;
//     if (terrain == 2) return night ? s->NightIce : s->DayIce;
//     return night ? s->NightWater : s->DayWater;
//   }
//   uint8_t raw = sample_2d(bmp, u_ratio, v_ratio);
//   return (GColor){.argb = raw | 0xC0};
// #else
//   bool white = s_world_bitmap ? (sample_2d(s_world_bitmap, u_ratio, v_ratio) == 0xFF) : true;
//   return (white != night) ? GColorWhite : GColorBlack;
// #endif
// }

// static void draw_marker(GContext *ctx, GPoint pt, GColor color) {
//   GColor border_color = (color.argb == GColorBlack.argb || 
//                          color.argb == GColorOxfordBlue.argb ||
//                          color.argb == GColorDarkGreen.argb) ? GColorWhite : GColorBlack;

//   graphics_context_set_fill_color(ctx, border_color);
//   graphics_fill_circle(ctx, pt, 4);

//   graphics_context_set_fill_color(ctx, color);
//   graphics_fill_circle(ctx, pt, 3);
// }

// static void draw_watch(Layer *layer, GContext *ctx) {
//   GRect bounds = layer_get_bounds(layer);
//   ClaySettings *s = settings_get();

//   // NOTE: Background fill removed. The layer is now transparent.

//   int32_t sun_lat_deg = 0;
//   int32_t sun_lon_deg = 0;
//   EarthVector sun = earth_sun(time(NULL) + s_time_offset, &sun_lat_deg, &sun_lon_deg);

//   int32_t view_lon = s->LongitudeOffset;
//   int16_t view_lat = (int16_t)s->LatitudeOffset;

//   if (s->CenterFocus == 1) {
//     view_lon = sun_lon_deg;
//   } else if (s->CenterFocus == 2) {
//     view_lon = earth_wrap_deg(sun_lon_deg + 180);
//   }

//   bool is_3d = (s->MapProjection == MAP_PROJECTION_3D);

//   ///////////////////////////////////////////////////////////////////////////////////////  
//   if (is_3d) {
//     // ======================== 3D GLOBE RENDERER ========================

//     int d = MAP_3D_GLOBE_DIAMETER_PX;
//     int R = d / 2;
//     int R2 = R * R;
//     int cx = bounds.size.w / 2;
//     int cy = g_dynamic_map_center_y; // Dynamically calculated center!    
    
//     int32_t zoom = s->AltitudeZoom;
//     if (zoom < 100) zoom = 100;
//     ////////////////////////////////////////////////////////////////////////////

//     EarthCamera camera = earth_camera(view_lat, (int16_t)view_lon);
//     memset(s_previous_row, 2, sizeof(s_previous_row));

//     for (int y = -R; y <= R; y++) {
//       int screen_y = cy + y;
// //       int col_y = y + R;
//       int32_t y2 = y * y;
//       if (y2 > R2) continue;

//       int32_t h_radius = earth_int_sqrt(R2 - y2);
//       int left_pixel_night = 2;

//       for (int x = -h_radius; x <= h_radius; x++) {
//         int screen_x = cx + x;
//         int col_x = x + R;

//         int32_t r2 = x * x + y2;
//         if (r2 > R2) {
//           s_previous_row[col_x] = 2;
//           left_pixel_night = 2;
//           continue;
//         }

//         // Apply zoom to screen rays
//         // (sx_z, sy_z) represent coordinates on the zoomed unit sphere
//         int32_t sx_unit = ((int64_t)x * FP_SCALE * 100) / (R * zoom);
//         int32_t sy_unit = ((int64_t)(-y) * FP_SCALE * 100) / (R * zoom);

//         int32_t r2_unit = ((int64_t)sx_unit * sx_unit + (int64_t)sy_unit * sy_unit) / FP_SCALE;
//         if (r2_unit > FP_SCALE) {
//           // Off the sphere horizon when zoomed in
//           s_previous_row[col_x] = 2;
//           left_pixel_night = 2;
//           continue;
//         }

//         int32_t sz_unit = earth_int_sqrt(FP_SCALE * FP_SCALE - ((int64_t)sx_unit * sx_unit + (int64_t)sy_unit * sy_unit));

//         // Transform into Earth vector P
//         EarthVector p;
//         p.x = (sx_unit * camera.right.x + sy_unit * camera.up.x + sz_unit * camera.forward.x) / FP_SCALE;
//         p.y = (sx_unit * camera.right.y + sy_unit * camera.up.y + sz_unit * camera.forward.y) / FP_SCALE;
//         p.z = (sx_unit * camera.right.z + sy_unit * camera.up.z + sz_unit * camera.forward.z) / FP_SCALE;

//         int32_t horiz = earth_int_sqrt(p.x * p.x + p.z * p.z);
//         int32_t lat_angle = (int16_t)atan2_lookup((int16_t)p.y, (int16_t)horiz);
        
//         int32_t v_numer = (int32_t)lat_angle * MAP_HEIGHT_3D;
//         int32_t v_denom = TRIG_MAX_ANGLE / 2;
//         int v_map = (MAP_HEIGHT_3D / 2) - ((v_numer + (v_numer >= 0 ? v_denom / 2 : -v_denom / 2)) / v_denom);
//         if (v_map < 0) v_map = 0;
//         if (v_map >= MAP_HEIGHT_3D) v_map = MAP_HEIGHT_3D - 1;

//         int32_t lon_angle = atan2_lookup((int16_t)p.z, (int16_t)p.x) + (TRIG_MAX_ANGLE / 2);
//         int32_t u_numer = (int64_t)lon_angle * MAP_WIDTH_3D;
//         int u_map = (u_numer + (TRIG_MAX_ANGLE / 2)) / TRIG_MAX_ANGLE;
//         u_map = u_map & (MAP_WIDTH_3D - 1);

//         uint8_t terrain = get_terrain_3d(u_map, v_map);

//         bool is_night = (earth_dot(p, sun) < 0);
//         bool is_edge = (left_pixel_night != 2 && left_pixel_night != is_night) ||
//                        (s_previous_row[col_x] != 2 && s_previous_row[col_x] != is_night);

//         GColor pixel_color;
//         if (is_edge) {
//           pixel_color = GColorWhite;
//         } else {
//           if (terrain == 1) pixel_color = is_night ? s->NightLand : s->DayLand;
//           else if (terrain == 2) pixel_color = is_night ? s->NightIce : s->DayIce;
//           else pixel_color = is_night ? s->NightWater : s->DayWater;
//         }

//         graphics_context_set_stroke_color(ctx, pixel_color);
//         graphics_draw_pixel(ctx, GPoint(screen_x, screen_y));

//         s_previous_row[col_x] = is_night ? 1 : 0;
//         left_pixel_night = is_night ? 1 : 0;
//       }
//     }

//     // 3D Location Marker (Zoom aware)
//     if (s->MarkerEnabled) {
//       int32_t m_lat_angle = ((int32_t)s->MarkerLat * TRIG_MAX_ANGLE) / 360;
//       int32_t m_lon_angle = ((int32_t)s->MarkerLon * TRIG_MAX_ANGLE) / 360;
//       int32_t m_cl = (int32_t)cos_lookup(m_lat_angle) / 4;
//       int32_t m_sl = (int32_t)sin_lookup(m_lat_angle) / 4;
//       int32_t m_so = (int32_t)sin_lookup(m_lon_angle) / 4;
//       int32_t m_co = (int32_t)cos_lookup(m_lon_angle) / 4;

//       EarthVector m_p;
//       m_p.x = (m_cl * m_co) / FP_SCALE;
//       m_p.y = m_sl;
//       m_p.z = (m_cl * m_so) / FP_SCALE;

//       if (earth_dot(m_p, camera.forward) > 0) {
//         int32_t m_sx = earth_dot(m_p, camera.right);
//         int32_t m_sy = earth_dot(m_p, camera.up);

//         int dot_x = cx + (int)(((int64_t)m_sx * R * zoom) / ((int64_t)FP_SCALE * FP_SCALE * 100));
//         int dot_y = cy - (int)(((int64_t)m_sy * R * zoom) / ((int64_t)FP_SCALE * FP_SCALE * 100));

//         // Only draw if inside the visible disk bounds
//         int r_dist2 = (dot_x - cx) * (dot_x - cx) + (dot_y - cy) * (dot_y - cy);
//         if (r_dist2 <= R2) {
//           draw_marker(ctx, GPoint(dot_x, dot_y), s->MarkerColor);
//         }
//       }
//     }

//   } else {
//     // ======================== 2D MAP RENDERER (200x100) ========================
//     int w = MAP_2D_PROJECTION_WIDTH_PX;
//     int h = MAP_2D_PROJECTION_HEIGHT_PX;
//     int start_x = (bounds.size.w - w) / 2;
//     int start_y = g_dynamic_map_center_y - (h / 2); // Dynamically calculated center!
//     // ...    

//     memset(s_previous_row, 2, sizeof(s_previous_row));

//     for (int y = 0; y < h; ++y) {
//       int left_pixel_night = 2;
//       int32_t lat_deg = 90 - (y * 180) / h;
//       int32_t lat_angle = (lat_deg * TRIG_MAX_ANGLE) / 360;
//       int32_t cl = (int32_t)cos_lookup(lat_angle) / 4;
//       int32_t sl = (int32_t)sin_lookup(lat_angle) / 4;

//       int32_t v_ratio = (y * FP_SCALE) / h;

//       for (int x = 0; x < w; ++x) {
//         int32_t lon_deg = earth_wrap_deg(view_lon + ((x * 360) / w - 180));
//         int32_t lon_angle = (lon_deg * TRIG_MAX_ANGLE) / 360;
//         int32_t so = (int32_t)sin_lookup(lon_angle) / 4;
//         int32_t co = (int32_t)cos_lookup(lon_angle) / 4;

//         EarthVector p;
//         p.x = (cl * co) / FP_SCALE;
//         p.y = sl;
//         p.z = (cl * so) / FP_SCALE;

//         int32_t u_ratio = (((lon_deg + 180) * FP_SCALE) / 360) & (FP_SCALE - 1);

//         bool is_night = (earth_dot(p, sun) < 0);
//         bool is_edge = (left_pixel_night != 2 && left_pixel_night != is_night) ||
//                        (s_previous_row[x] != 2 && s_previous_row[x] != is_night);

//         GColor pixel_color;
//         if (is_edge) {
//           pixel_color = GColorWhite;
//         } else {
//           pixel_color = pixel_color_2d(s, u_ratio, v_ratio, is_night);
//         }

//         graphics_context_set_stroke_color(ctx, pixel_color);
//         graphics_draw_pixel(ctx, GPoint(start_x + x, start_y + y));

//         s_previous_row[x] = is_night ? 1 : 0;
//         left_pixel_night = is_night ? 1 : 0;
//       }
//     }

//     if (s->MarkerEnabled) {
//       int32_t rel_lon = earth_wrap_deg(s->MarkerLon - view_lon);
//       int dot_x = start_x + ((rel_lon + 180) * w) / 360;
//       int dot_y = start_y + ((90 - s->MarkerLat) * h) / 180;

//       if (dot_x >= start_x && dot_x < start_x + w &&
//           dot_y >= start_y && dot_y < start_y + h) {
//         draw_marker(ctx, GPoint(dot_x, dot_y), s->MarkerColor);
//       }
//     }
//   }
// }

// void map_init(Layer *parent_layer, GRect bounds) {
//   s_canvas = layer_create(bounds);
//   if (!s_canvas) return;
//   layer_set_update_proc(s_canvas, draw_watch);
//   layer_add_child(parent_layer, s_canvas);
// }

// void map_deinit(void) {
//   if (s_canvas) layer_destroy(s_canvas);
//   s_canvas = NULL;
//   if (s_world_bitmap) gbitmap_destroy(s_world_bitmap);
//   s_world_bitmap = NULL;
// #ifdef PBL_COLOR
//   if (s_night_bitmap) gbitmap_destroy(s_night_bitmap);
//   s_night_bitmap = NULL;
// #endif
//   s_day_index = s_night_index = -1;
// }

// void map_set_time_offset(int offset) { s_time_offset = offset; }
// void map_force_redraw(void) { if (s_canvas) layer_mark_dirty(s_canvas); }

// void map_reload_bitmaps(int day_index, int night_index) {
//   ClaySettings *s = settings_get();

//   if (s->MapProjection == MAP_PROJECTION_3D) {
//     if (s_world_bitmap) { gbitmap_destroy(s_world_bitmap); s_world_bitmap = NULL; }
// #ifdef PBL_COLOR
//     if (s_night_bitmap) { gbitmap_destroy(s_night_bitmap); s_night_bitmap = NULL; }
// #endif
//     s_day_index = s_night_index = -1;
//     return;
//   }

// #ifdef PBL_COLOR
//   if (day_index != s_day_index || !s_world_bitmap) {
//     if (s_world_bitmap) gbitmap_destroy(s_world_bitmap);
//     uint32_t id = RESOURCE_ID_DAY_01_CHARLIE;
//     if (day_index == 1) id = RESOURCE_ID_DAY_02_BLUE_MARBLE;
//     if (day_index == 2) id = RESOURCE_ID_3_Color_Map;
//     s_world_bitmap = gbitmap_create_with_resource(id);
//     s_day_index = day_index;
//   }
//   if (night_index != s_night_index || !s_night_bitmap) {
//     if (s_night_bitmap) gbitmap_destroy(s_night_bitmap);
//     uint32_t id = RESOURCE_ID_NIGHT_01_DITHER;
//     if (night_index == 1) id = RESOURCE_ID_NIGHT_02_CLEAN;
//     if (night_index == 2) id = RESOURCE_ID_3_Color_Map;
//     s_night_bitmap = gbitmap_create_with_resource(id);
//     s_night_index = night_index;
//   }
// #else
//   (void)day_index; (void)night_index;
//   if (!s_world_bitmap) s_world_bitmap = gbitmap_create_with_resource(RESOURCE_ID_WORLD);
// #endif
// }
