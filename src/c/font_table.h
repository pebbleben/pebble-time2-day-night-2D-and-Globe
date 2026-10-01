#pragma once
#include <pebble.h>

// ============================================================================
// MASTER FONT CALIBRATION TABLE
// Edit any number here to adjust spacing for that specific font.
// ============================================================================

// ---------------- 1. TIME FONT METRICS (True Visual Heights) ----------------
#define TIME_OSWALD_HEIGHT          52   // Oswald Bold 54 (Tallest)
#define TIME_BARLOW_HEIGHT          50   // Barlow Bold 62
#define TIME_BITHAM_HEIGHT          40   // Bitham 42 Bold
#define TIME_ROBOTO_HEIGHT          44   // Roboto Bold 49
#define TIME_SERIF_HEIGHT           30   // Droid Serif 28
#define TIME_LECO_HEIGHT            40   // Default Leco 42 Numbers

// Fine vertical nudges for Time fonts (positive = down, negative = up)
#define TIME_OSWALD_NUDGE_Y        -3 // -2
#define TIME_BARLOW_NUDGE_Y        -12 //-3
#define TIME_BITHAM_NUDGE_Y        -4 //1 
#define TIME_ROBOTO_NUDGE_Y         0
#define TIME_SERIF_NUDGE_Y          1 //4
#define TIME_LECO_NUDGE_Y           -5 // 0

// ---------------- 2. DATE FONT METRICS (Includes Full 'p'/'y' Descenders) ---
#define DATE_BITHAM_HEIGHT          36   // Bitham 30 Black (Generous space for 'p'!)
#define DATE_GOTHIC28_HEIGHT        30   // Gothic 28 Bold
#define DATE_SERIF_HEIGHT           30   // Droid Serif 28
#define DATE_GOTHIC24_HEIGHT        24   // Gothic 24 Bold
#define DATE_SMALL_HEIGHT           18   // Gothic 18 Bold

// Fine vertical lift for Date fonts (negative lifts UP off the glass)
#define DATE_BITHAM_LIFT_Y         2 // -6   // Lifts Bitham 30 up so 'p' never touches bezel
#define DATE_GOTHIC28_LIFT_Y       0 //-4
#define DATE_SERIF_LIFT_Y          1 // -4
#define DATE_GOTHIC24_LIFT_Y       -2
#define DATE_SMALL_LIFT_Y           0

// ---------------- 3. MASTER BEZEL MARGINS -----------------------------------
#define MASTER_TOP_BEZEL_PADDING_PX     2 //4   // Gap from top screen to time
#define MASTER_BOTTOM_BEZEL_PADDING_PX  4 //10   // Safety buffer from date to bottom glass

// ============================================================================
// HELPER LOOKUP FUNCTIONS
// ============================================================================

static inline int font_table_get_time_height(const char* font_name) {
  if (strcmp(font_name, "oswald") == 0) return TIME_OSWALD_HEIGHT;
  if (strcmp(font_name, "barlow") == 0) return TIME_BARLOW_HEIGHT;
  if (strcmp(font_name, "bitham") == 0) return TIME_BITHAM_HEIGHT;
  if (strcmp(font_name, "roboto") == 0) return TIME_ROBOTO_HEIGHT;
  if (strcmp(font_name, "serif") == 0)  return TIME_SERIF_HEIGHT;
  return TIME_LECO_HEIGHT;
}

static inline int font_table_get_time_nudge(const char* font_name) {
  if (strcmp(font_name, "oswald") == 0) return TIME_OSWALD_NUDGE_Y;
  if (strcmp(font_name, "barlow") == 0) return TIME_BARLOW_NUDGE_Y;
  if (strcmp(font_name, "bitham") == 0) return TIME_BITHAM_NUDGE_Y;
  if (strcmp(font_name, "roboto") == 0) return TIME_ROBOTO_NUDGE_Y;
  if (strcmp(font_name, "serif") == 0)  return TIME_SERIF_NUDGE_Y;
  return TIME_LECO_NUDGE_Y;
}

static inline int font_table_get_date_height(const char* font_name) {
  if (strcmp(font_name, "bitham") == 0)   return DATE_BITHAM_HEIGHT;
  if (strcmp(font_name, "gothic28") == 0) return DATE_GOTHIC28_HEIGHT;
  if (strcmp(font_name, "serif") == 0)    return DATE_SERIF_HEIGHT;
  if (strcmp(font_name, "gothic24") == 0) return DATE_GOTHIC24_HEIGHT;
  return DATE_SMALL_HEIGHT;
}

static inline int font_table_get_date_lift(const char* font_name) {
  if (strcmp(font_name, "bitham") == 0)   return DATE_BITHAM_LIFT_Y;
  if (strcmp(font_name, "gothic28") == 0) return DATE_GOTHIC28_LIFT_Y;
  if (strcmp(font_name, "serif") == 0)    return DATE_SERIF_LIFT_Y;
  if (strcmp(font_name, "gothic24") == 0) return DATE_GOTHIC24_LIFT_Y;
  return DATE_SMALL_LIFT_Y;
}