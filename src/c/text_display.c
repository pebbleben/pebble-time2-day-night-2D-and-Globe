#include <pebble.h>
#include "text_display.h"
#include "layout.h"
#include "settings.h"
#include "font_table.h"

#define SCREENSHOT_MODE 0

int g_dynamic_map_center_y = 120;

static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
static Layer *s_battery_layer;
static BatteryChargeState s_battery_state;
static GColor s_fg_color;

static GFont s_oswald_time_font;
static GFont s_barlow_time_font;

static GRect s_bounds;

static GFont get_time_font(const char* font_name) {
  if (strcmp(font_name, "oswald") == 0 && s_oswald_time_font) return s_oswald_time_font;
  if (strcmp(font_name, "barlow") == 0 && s_barlow_time_font) return s_barlow_time_font;
  if (strcmp(font_name, "bitham") == 0) return fonts_get_system_font(FONT_KEY_BITHAM_42_MEDIUM_NUMBERS);
  if (strcmp(font_name, "serif") == 0)  return fonts_get_system_font(FONT_KEY_DROID_SERIF_28_BOLD);
  if (strcmp(font_name, "roboto") == 0) {
#if PBL_DISPLAY_HEIGHT >= 200
    return fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49);
#else
    return fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT); 
#endif
  }
  return fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
}

static GFont get_date_font(const char* font_name) {
  if (strcmp(font_name, "bitham") == 0)   return fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK);
  if (strcmp(font_name, "gothic28") == 0) return fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  if (strcmp(font_name, "serif") == 0)    return fonts_get_system_font(FONT_KEY_DROID_SERIF_28_BOLD);
  if (strcmp(font_name, "gothic24") == 0) return fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  return fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
}

// Battery indicator
static void battery_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_stroke_color(ctx, s_fg_color);
  graphics_context_set_fill_color(ctx, s_fg_color);

  int bat_w = HEADER_BATTERY_WIDTH_PX;
  int bat_h = HEADER_BATTERY_HEIGHT_PX;
  int bat_x = bounds.size.w - bat_w - HEADER_BATTERY_X_AXIS_RIGHT_MARGIN_PX + HEADER_BATTERY_NUDGE_X_PX;
  int bat_y = HEADER_BATTERY_Y_AXIS_TOP_PX + HEADER_BATTERY_NUDGE_Y_PX;

  graphics_draw_rect(ctx, GRect(bat_x, bat_y, bat_w, bat_h));
  graphics_fill_rect(ctx, GRect(bat_x + bat_w, bat_y + (bat_h / 2) - 2, 2, 4), 0, GCornerNone);

  int max_inner_w = bat_w - 4;
  int charge_w = (s_battery_state.charge_percent * max_inner_w) / 100;
  
  if (charge_w > 0) {
    graphics_fill_rect(ctx, GRect(bat_x + 2, bat_y + 2, charge_w, bat_h - 4), 0, GCornerNone);
  }

  if (s_battery_state.is_charging) {
    graphics_draw_line(ctx, GPoint(bat_x - 6, bat_y + 3), GPoint(bat_x - 6, bat_y + 7));
    graphics_draw_line(ctx, GPoint(bat_x - 8, bat_y + 5), GPoint(bat_x - 4, bat_y + 5));
  }
}

// Dynamic layout engine driven by font_table.h and layout.h
void text_display_update_fonts(void) {
  ClaySettings *settings = settings_get();
  int H = s_bounds.size.h;
  int W = s_bounds.size.w;

  bool is_3d = (settings->MapProjection == MAP_PROJECTION_3D);
  int map_h = is_3d ? MAP_3D_GLOBE_DIAMETER_PX : MAP_2D_PROJECTION_HEIGHT_PX;

  int time_h = font_table_get_time_height(settings->TimeFont);
  int date_h = font_table_get_date_height(settings->DateFont);
  int time_nudge = font_table_get_time_nudge(settings->TimeFont);
  int date_lift  = font_table_get_date_lift(settings->DateFont);

  int total_blank = H - time_h - map_h - date_h - MASTER_TOP_BEZEL_PADDING_PX - MASTER_BOTTOM_BEZEL_PADDING_PX;

  int time_y;
  int date_y;
  int map_center_y;

  if (total_blank >= 8) {
    // Distribute blank space: 40% to outer bezels, 60% as cushions around the Earth
    int gap_around_globe = ((total_blank * 60) / 100) / 2;
    int top_extra_gap    = ((total_blank * 40) / 100) / 2;

    time_y = MASTER_TOP_BEZEL_PADDING_PX + top_extra_gap + time_nudge;

    int map_top_y = MASTER_TOP_BEZEL_PADDING_PX + top_extra_gap + time_h + gap_around_globe;
    map_center_y  = map_top_y + (map_h / 2);

    int map_bottom_y = map_top_y + map_h;
    date_y = map_bottom_y + gap_around_globe + date_lift;

    if (date_y + date_h > H - MASTER_BOTTOM_BEZEL_PADDING_PX) {
      date_y = H - date_h - MASTER_BOTTOM_BEZEL_PADDING_PX;
    }
  } else {
    // Tight overlap
    time_y = MASTER_TOP_BEZEL_PADDING_PX + time_nudge;
    date_y = H - date_h - MASTER_BOTTOM_BEZEL_PADDING_PX + date_lift;

    int available_middle = date_y - (time_y + time_h);
    map_center_y = (time_y + time_h) + (available_middle / 2);
  }

  // Update dynamic baseline center for map.c
  g_dynamic_map_center_y = map_center_y;

  // Position Time Layer
  text_layer_set_font(s_time_layer, get_time_font(settings->TimeFont));
  const char *saved_time = text_layer_get_text(s_time_layer);
  if (!saved_time || strlen(saved_time) == 0) {
    text_layer_set_text(s_time_layer, "12:59");
  }
  layer_set_frame(text_layer_get_layer(s_time_layer), GRect(TIME_LAYER_NUDGE_X_PX, time_y, W, time_h + 20));
  text_layer_set_text(s_time_layer, saved_time);

  // Position Date Layer
  text_layer_set_font(s_date_layer, get_date_font(settings->DateFont));
  const char *saved_date = text_layer_get_text(s_date_layer);
  layer_set_frame(text_layer_get_layer(s_date_layer), GRect(DATE_LAYER_NUDGE_X_PX, date_y, W, date_h + 16));
  text_layer_set_text(s_date_layer, saved_date);
}

void text_display_init(Layer *parent_layer, GRect bounds) {
  s_bounds = bounds;
  int W = bounds.size.w;
  s_fg_color = GColorWhite;

  s_time_layer = text_layer_create(GRect(0, 0, W, 64));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(parent_layer, text_layer_get_layer(s_time_layer));

  s_date_layer = text_layer_create(GRect(0, 0, W, 52));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_date_layer, GTextOverflowModeFill);
  text_layer_set_text(s_date_layer, "Wed, Sep 30"); 
  layer_add_child(parent_layer, text_layer_get_layer(s_date_layer));

  s_battery_layer = layer_create(GRect(0, 0, W, 30));
  layer_set_update_proc(s_battery_layer, battery_update_proc);
  layer_add_child(parent_layer, s_battery_layer);
  
#ifdef RESOURCE_ID_FONT_OSWALD_BOLD_54
  s_oswald_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_OSWALD_BOLD_54));
#endif
#ifdef RESOURCE_ID_FONT_Barlow_Semi_Bold_62
  s_barlow_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_Barlow_Semi_Bold_62));
#endif

  text_display_update_fonts();
}

void text_display_deinit(void) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);
  layer_destroy(s_battery_layer);
  if (s_oswald_time_font) fonts_unload_custom_font(s_oswald_time_font);
  if (s_barlow_time_font) fonts_unload_custom_font(s_barlow_time_font);
}

void text_display_update_time(struct tm *tick_time) {
#if SCREENSHOT_MODE
  text_layer_set_text(s_time_layer, "10:09");
  text_layer_set_text(s_date_layer, "Wed, Sep 30");
#else
  static char time_text[] = "00:00xx";
  static char date_text[16];

  strftime(date_text, sizeof(date_text), "%a, %b %e", tick_time);
  text_layer_set_text(s_date_layer, date_text);

  strftime(time_text, sizeof(time_text),
           clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  if (!clock_is_24h_style() && time_text[0] == '0') {
    memmove(time_text, &time_text[1], sizeof(time_text) - 1);
  }
  text_layer_set_text(s_time_layer, time_text);
#endif
}

void text_display_update_battery(BatteryChargeState charge_state) {
#if SCREENSHOT_MODE
  s_battery_state.charge_percent = 100;
  s_battery_state.is_charging = false;
#else
  s_battery_state = charge_state;
#endif
  if (s_battery_layer) layer_mark_dirty(s_battery_layer);
}

void text_display_apply_colors(GColor fg_color) {
  s_fg_color = fg_color;
  text_layer_set_text_color(s_time_layer, fg_color);
  text_layer_set_text_color(s_date_layer, fg_color);
  if (s_battery_layer) layer_mark_dirty(s_battery_layer);
}

// #include <pebble.h>
// #include "text_display.h"
// #include "layout.h"
// #include "settings.h"
// #include "font_table.h"

// #define SCREENSHOT_MODE 0

// int g_dynamic_map_center_y = 120;

// static TextLayer *s_time_layer;
// static TextLayer *s_date_layer;
// static Layer *s_battery_layer;
// static BatteryChargeState s_battery_state;
// static GColor s_fg_color;

// static GFont s_oswald_time_font;
// static GFont s_barlow_time_font;

// static GRect s_bounds;

// // Font loaders
// static GFont get_time_font(const char* font_name) {
//   if (strcmp(font_name, "oswald") == 0 && s_oswald_time_font) return s_oswald_time_font;
//   if (strcmp(font_name, "barlow") == 0 && s_barlow_time_font) return s_barlow_time_font;
//   if (strcmp(font_name, "bitham") == 0) return fonts_get_system_font(FONT_KEY_BITHAM_42_MEDIUM_NUMBERS);
//   if (strcmp(font_name, "serif") == 0)  return fonts_get_system_font(FONT_KEY_DROID_SERIF_28_BOLD);
//   if (strcmp(font_name, "roboto") == 0) {
// #if PBL_DISPLAY_HEIGHT >= 200
//     return fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49);
// #else
//     return fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT); 
// #endif
//   }
//   return fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
// }

// static GFont get_date_font(const char* font_name) {
//   if (strcmp(font_name, "bitham") == 0)   return fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK);
//   if (strcmp(font_name, "gothic28") == 0) return fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
//   if (strcmp(font_name, "serif") == 0)    return fonts_get_system_font(FONT_KEY_DROID_SERIF_28_BOLD);
//   if (strcmp(font_name, "gothic24") == 0) return fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
//   return fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
// }

// // Battery indicator
// static void battery_update_proc(Layer *layer, GContext *ctx) {
//   GRect bounds = layer_get_bounds(layer);
//   graphics_context_set_stroke_color(ctx, s_fg_color);
//   graphics_context_set_fill_color(ctx, s_fg_color);

//   int bat_w = HEADER_BATTERY_WIDTH_PX;
//   int bat_h = HEADER_BATTERY_HEIGHT_PX;
//   int bat_x = bounds.size.w - bat_w - HEADER_BATTERY_X_AXIS_RIGHT_MARGIN_PX;
//   int bat_y = HEADER_BATTERY_Y_AXIS_TOP_PX;

//   graphics_draw_rect(ctx, GRect(bat_x, bat_y, bat_w, bat_h));
//   graphics_fill_rect(ctx, GRect(bat_x + bat_w, bat_y + (bat_h / 2) - 2, 2, 4), 0, GCornerNone);

//   int max_inner_w = bat_w - 4;
//   int charge_w = (s_battery_state.charge_percent * max_inner_w) / 100;
  
//   if (charge_w > 0) {
//     graphics_fill_rect(ctx, GRect(bat_x + 2, bat_y + 2, charge_w, bat_h - 4), 0, GCornerNone);
//   }

//   if (s_battery_state.is_charging) {
//     graphics_draw_line(ctx, GPoint(bat_x - 6, bat_y + 3), GPoint(bat_x - 6, bat_y + 7));
//     graphics_draw_line(ctx, GPoint(bat_x - 8, bat_y + 5), GPoint(bat_x - 4, bat_y + 5));
//   }
// }

// // Dynamic layout engine driven by font_table.h
// void text_display_update_fonts(void) {
//   ClaySettings *settings = settings_get();
//   int H = s_bounds.size.h;
//   int W = s_bounds.size.w;

//   bool is_3d = (settings->MapProjection == MAP_PROJECTION_3D);
//   int map_h = is_3d ? MAP_3D_GLOBE_DIAMETER_PX : MAP_2D_PROJECTION_HEIGHT_PX;

//   // 1. Read calibrated metrics directly from font_table.h
//   int time_h = font_table_get_time_height(settings->TimeFont);
//   int date_h = font_table_get_date_height(settings->DateFont);
//   int time_nudge = font_table_get_time_nudge(settings->TimeFont);
//   int date_lift  = font_table_get_date_lift(settings->DateFont);

//   // 2. Calculate remaining blank space
//   int total_blank = H - time_h - map_h - date_h - MASTER_TOP_BEZEL_PADDING_PX - MASTER_BOTTOM_BEZEL_PADDING_PX;

//   int time_y;
//   int date_y;
//   int map_center_y;

//   if (total_blank >= 8) {
//     // Distribute blank space: 40% to outer bezels, 60% as cushions around the Earth
//     int gap_around_globe = ((total_blank * 60) / 100) / 2;
//     int top_extra_gap    = ((total_blank * 40) / 100) / 2;

//     time_y = MASTER_TOP_BEZEL_PADDING_PX + top_extra_gap + time_nudge;

//     int map_top_y = MASTER_TOP_BEZEL_PADDING_PX + top_extra_gap + time_h + gap_around_globe;
// //     map_center_y  = map_top_y + (map_h / 2);
//     map_center_y  = map_top_y + (map_h / 2) + 4;

//     int map_bottom_y = map_top_y + map_h;
//     date_y = map_bottom_y + gap_around_globe + date_lift;

//     // Safety guard: ensure the date never drops below the bottom cushion
//     if (date_y + date_h > H - MASTER_BOTTOM_BEZEL_PADDING_PX) {
//       date_y = H - date_h - MASTER_BOTTOM_BEZEL_PADDING_PX;
//     }
//   } else {
//     // TIGHT OVERLAP (Very large fonts)
//     time_y = MASTER_TOP_BEZEL_PADDING_PX + time_nudge;
//     date_y = H - date_h - MASTER_BOTTOM_BEZEL_PADDING_PX + date_lift;

//     int available_middle = date_y - (time_y + time_h);
// //     map_center_y = (time_y + time_h) + (available_middle / 2);
//     map_center_y = (time_y + time_h) + (available_middle / 2) + 4;
//   }

//   // Update dynamic center for map.c
//   g_dynamic_map_center_y = map_center_y;

//   // Position Time Layer
//   text_layer_set_font(s_time_layer, get_time_font(settings->TimeFont));
//   const char *saved_time = text_layer_get_text(s_time_layer);
//   if (!saved_time || strlen(saved_time) == 0) {
//     text_layer_set_text(s_time_layer, "12:59");
//   }
//   layer_set_frame(text_layer_get_layer(s_time_layer), GRect(0, time_y, W, time_h + 20));
//   text_layer_set_text(s_time_layer, saved_time);

//   // Position Date Layer
//   text_layer_set_font(s_date_layer, get_date_font(settings->DateFont));
//   const char *saved_date = text_layer_get_text(s_date_layer);
//   // Full layer height gives descenders room so 'p' never clips
//   layer_set_frame(text_layer_get_layer(s_date_layer), GRect(0, date_y, W, date_h + 16));
//   text_layer_set_text(s_date_layer, saved_date);
// }

// void text_display_init(Layer *parent_layer, GRect bounds) {
//   s_bounds = bounds;
//   int W = bounds.size.w;
//   s_fg_color = GColorWhite;

//   s_time_layer = text_layer_create(GRect(0, 0, W, 64));
//   text_layer_set_background_color(s_time_layer, GColorClear);
//   text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
//   layer_add_child(parent_layer, text_layer_get_layer(s_time_layer));

//   s_date_layer = text_layer_create(GRect(0, 0, W, 52));
//   text_layer_set_background_color(s_date_layer, GColorClear);
//   text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
//   text_layer_set_overflow_mode(s_date_layer, GTextOverflowModeFill);
//   text_layer_set_text(s_date_layer, "Wed, Sep 30"); 
//   layer_add_child(parent_layer, text_layer_get_layer(s_date_layer));

//   s_battery_layer = layer_create(GRect(0, 0, W, 30));
//   layer_set_update_proc(s_battery_layer, battery_update_proc);
//   layer_add_child(parent_layer, s_battery_layer);
  
// #ifdef RESOURCE_ID_FONT_OSWALD_BOLD_54
//   s_oswald_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_OSWALD_BOLD_54));
// #endif
// #ifdef RESOURCE_ID_FONT_Barlow_Semi_Bold_62
//   s_barlow_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_Barlow_Semi_Bold_62));
// #endif

//   text_display_update_fonts();
// }

// void text_display_deinit(void) {
//   text_layer_destroy(s_time_layer);
//   text_layer_destroy(s_date_layer);
//   layer_destroy(s_battery_layer);
//   if (s_oswald_time_font) fonts_unload_custom_font(s_oswald_time_font);
//   if (s_barlow_time_font) fonts_unload_custom_font(s_barlow_time_font);
// }

// void text_display_update_time(struct tm *tick_time) {
// #if SCREENSHOT_MODE
//   text_layer_set_text(s_time_layer, "10:09");
//   text_layer_set_text(s_date_layer, "Wed, Sep 30");
// #else
//   static char time_text[] = "00:00xx";
//   static char date_text[16];

//   strftime(date_text, sizeof(date_text), "%a, %b %e", tick_time);
//   text_layer_set_text(s_date_layer, date_text);

//   strftime(time_text, sizeof(time_text),
//            clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
//   if (!clock_is_24h_style() && time_text[0] == '0') {
//     memmove(time_text, &time_text[1], sizeof(time_text) - 1);
//   }
//   text_layer_set_text(s_time_layer, time_text);
// #endif
// }

// void text_display_update_battery(BatteryChargeState charge_state) {
// #if SCREENSHOT_MODE
//   s_battery_state.charge_percent = 100;
//   s_battery_state.is_charging = false;
// #else
//   s_battery_state = charge_state;
// #endif
//   if (s_battery_layer) layer_mark_dirty(s_battery_layer);
// }

// void text_display_apply_colors(GColor fg_color) {
//   s_fg_color = fg_color;
//   text_layer_set_text_color(s_time_layer, fg_color);
//   text_layer_set_text_color(s_date_layer, fg_color);
//   if (s_battery_layer) layer_mark_dirty(s_battery_layer);
// }

