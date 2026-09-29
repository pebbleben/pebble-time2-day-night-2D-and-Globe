#include <pebble.h>
#include "map.h"
#include "layout.h"
#include "settings.h"
#include "earth_math.h"
#include "earth_map.h"

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

// Fast unpack of 2-bit terrain from Flash: 0=Water, 1=Land, 2=Ice
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

static void draw_watch(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GRect band = GRect(0, bounds.size.h * MAP_TOP_168 / LAYOUT_H,
                     bounds.size.w, bounds.size.h * MAP_H_168 / LAYOUT_H);

  ClaySettings *s = settings_get();

  graphics_context_set_fill_color(ctx, s->BackgroundColor);
  graphics_fill_rect(ctx, band, 0, GCornerNone);

  // Solar calculations in UTC
  int32_t sun_lat_deg = 0;
  int32_t sun_lon_deg = 0;
  EarthVector sun = earth_sun(time(NULL) + s_time_offset, &sun_lat_deg, &sun_lon_deg);

  // Focus longitude:
  // 0 = Fixed location (Sliders)
  // 1 = Center on Day (Follow Sun)
  // 2 = Center on Night (Follow Midnight Antipode)
  int32_t view_lon = s->LongitudeOffset;
  int16_t view_lat = (int16_t)s->LatitudeOffset;

  if (s->CenterFocus == 1) {
    view_lon = sun_lon_deg;
  } else if (s->CenterFocus == 2) {
    view_lon = earth_wrap_deg(sun_lon_deg + 180);
  }

  APP_LOG(APP_LOG_LEVEL_INFO, "EARTH VIEW: proj=%d focus=%d sun_lon=%d view_lon=%d", 
          s->MapProjection, s->CenterFocus, (int)sun_lon_deg, (int)view_lon);

  bool is_3d = (s->MapProjection == MAP_PROJECTION_3D);
  GRect area = band;

  if (is_3d) {
    int d = (band.size.w < band.size.h ? band.size.w : band.size.h) - 2;
    if (d < 2) return;
    area = GRect(band.origin.x + (band.size.w - d) / 2,
                 band.origin.y + (band.size.h - d) / 2, d, d);
  } else {
    int w = band.size.w;
    if (w > 2 * band.size.h) w = 2 * band.size.h;
    w -= w % 2;
    area = GRect(band.origin.x + (band.size.w - w) / 2,
                 band.origin.y + (band.size.h - w / 2) / 2, w, w / 2);
  }

  if (area.size.w < 2 || area.size.h < 1 || area.size.w > MAX_MAP_WIDTH) return;

  memset(s_previous_row, 2, sizeof(s_previous_row));

  if (is_3d) {
    // ======================== 3D GLOBE RENDERER ========================
    EarthCamera camera = earth_camera(view_lat, (int16_t)view_lon);
    int R = area.size.w / 2;
    int R2 = R * R;
    int cx = area.origin.x + R;
    int cy = area.origin.y + R;

    for (int y = -R; y <= R; y++) {
      int screen_y = cy + y;
      int col_y = y + R;
      int32_t y2 = y * y;
      if (y2 > R2) continue;

      int32_t h_radius = earth_int_sqrt(R2 - y2);
      int32_t sy_unit = (-y * FP_SCALE) / R;
      int left_pixel_night = 2;

      for (int x = -h_radius; x <= h_radius; x++) {
        int screen_x = cx + x;
        int col_x = x + R;

        int32_t r2 = x * x + y2;
        if (r2 > R2) {
          s_previous_row[col_x] = 2;
          left_pixel_night = 2;
          continue;
        }

        int32_t Z_v = earth_int_sqrt(R2 - r2);
        int32_t sx_unit = (x * FP_SCALE) / R;
        int32_t sz_unit = (Z_v * FP_SCALE) / R;

        EarthVector p;
        p.x = (sx_unit * camera.right.x + sy_unit * camera.up.x + sz_unit * camera.forward.x) / FP_SCALE;
        p.y = (sx_unit * camera.right.y + sy_unit * camera.up.y + sz_unit * camera.forward.y) / FP_SCALE;
        p.z = (sx_unit * camera.right.z + sy_unit * camera.up.z + sz_unit * camera.forward.z) / FP_SCALE;

        int32_t horiz = earth_int_sqrt(p.x * p.x + p.z * p.z);
        int32_t lat_angle = (int16_t)atan2_lookup((int16_t)p.y, (int16_t)horiz);
        int v_map = (MAP_HEIGHT_3D / 2) - (int)(((int32_t)lat_angle * MAP_HEIGHT_3D) / (TRIG_MAX_ANGLE / 2));
        if (v_map < 0) v_map = 0;
        if (v_map >= MAP_HEIGHT_3D) v_map = MAP_HEIGHT_3D - 1;

        int32_t lon_angle = atan2_lookup((int16_t)p.z, (int16_t)p.x) + (TRIG_MAX_ANGLE / 2);
        int u_map = (int)(((int64_t)lon_angle * MAP_WIDTH_3D) / TRIG_MAX_ANGLE);
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
  } else {
    // ======================== 2D MAP RENDERER ========================
    for (int y = 0; y < area.size.h; ++y) {
      int left_pixel_night = 2;
      int32_t lat_deg = 90 - (y * 180) / area.size.h;
      int32_t lat_angle = (lat_deg * TRIG_MAX_ANGLE) / 360;
      int32_t cl = (int32_t)cos_lookup(lat_angle) / 4;
      int32_t sl = (int32_t)sin_lookup(lat_angle) / 4;

      int32_t v_ratio = (y * FP_SCALE) / area.size.h;

      for (int x = 0; x < area.size.w; ++x) {
        int32_t lon_deg = earth_wrap_deg(view_lon + ((x * 360) / area.size.w - 180));
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
        graphics_draw_pixel(ctx, GPoint(area.origin.x + x, area.origin.y + y));

        s_previous_row[x] = is_night ? 1 : 0;
        left_pixel_night = is_night ? 1 : 0;
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