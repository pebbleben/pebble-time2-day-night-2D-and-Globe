#pragma once
#include <pebble.h>

#if PBL_DISPLAY_HEIGHT >= 200
  // ==========================================================================
  // PEBBLE TIME 2 (EMERY 200 x 228 px)
  // ==========================================================================
  #define DISPLAY_SCREEN_WIDTH_PX                      200
  #define DISPLAY_SCREEN_HEIGHT_PX                     228

  // ---------------- 3D Globe Map ----------------
  #define MAP_3D_GLOBE_DIAMETER_PX                     144 //128 //144 //136  // 116  // Radius = 58 px
  #define MAP_3D_NUDGE_X_PX                              0  // (+) right, (-) left
  #define MAP_3D_NUDGE_Y_PX                              6 //4  // (+) down,  (-) up (Previous hardcoded +4)

  // ---------------- 2D Cylindrical Map ----------
  #define MAP_2D_PROJECTION_WIDTH_PX                   200  // Full width
  #define MAP_2D_PROJECTION_HEIGHT_PX                  100  // 2:1 ratio
  #define MAP_2D_NUDGE_X_PX                              0  // (+) right, (-) left
  #define MAP_2D_NUDGE_Y_PX                              3  // (+) down,  (-) up

  // ---------------- Battery Indicator -----------
  #define HEADER_BATTERY_WIDTH_PX                       20
  #define HEADER_BATTERY_HEIGHT_PX                      10
  #define HEADER_BATTERY_Y_AXIS_TOP_PX                   3
  #define HEADER_BATTERY_X_AXIS_RIGHT_MARGIN_PX          4
  #define HEADER_BATTERY_NUDGE_X_PX                      0  // Fine adjustments / (+) right, (-) left
  #define HEADER_BATTERY_NUDGE_Y_PX                      0 // (+) down,  (-) up

  // ---------------- Master Text Layer Nudges ----
  #define TIME_LAYER_NUDGE_X_PX                          0
  #define DATE_LAYER_NUDGE_X_PX                          0



#else
  // ==========================================================================
  // PEBBLE TIME / STEEL / CLASSIC (BASALT 144 x 168 px)
  // ==========================================================================
  #define DISPLAY_SCREEN_WIDTH_PX                      144
  #define DISPLAY_SCREEN_HEIGHT_PX                     168

  // ---------------- 3D Globe Map ----------------
  #define MAP_3D_GLOBE_DIAMETER_PX                      80
  #define MAP_3D_NUDGE_X_PX                              0  // (+) right, (-) left
  #define MAP_3D_NUDGE_Y_PX                              2  // (+) down,  (-) up

  // ---------------- 2D Cylindrical Map ----------
  #define MAP_2D_PROJECTION_WIDTH_PX                   144
  #define MAP_2D_PROJECTION_HEIGHT_PX                   72
  #define MAP_2D_NUDGE_X_PX                              0  // (+) right, (-) left
  #define MAP_2D_NUDGE_Y_PX                              0  // (+) down,  (-) up

  // ---------------- Battery Indicator -----------
  #define HEADER_BATTERY_WIDTH_PX                       18
  #define HEADER_BATTERY_HEIGHT_PX                       9
  #define HEADER_BATTERY_Y_AXIS_TOP_PX                   2
  #define HEADER_BATTERY_X_AXIS_RIGHT_MARGIN_PX          3
  #define HEADER_BATTERY_NUDGE_X_PX                      0
  #define HEADER_BATTERY_NUDGE_Y_PX                      0

  // ---------------- Master Text Layer Nudges ----
  #define TIME_LAYER_NUDGE_X_PX                          0
  #define DATE_LAYER_NUDGE_X_PX                          0


#endif

// Shared dynamic map center line calculated on the fly by font choice
extern int g_dynamic_map_center_y;



////////////////////////////////////////////////////////////////////////////////////////////////
// #pragma once
// #include <pebble.h>

// // =============================================================================
// // LAYOUT & COMPONENT POSITIONING CONFIGURATION
// // =============================================================================

// #if PBL_DISPLAY_HEIGHT >= 200
//   // ---------------------------------------------------------------------------
//   // PLATFORM: PEBBLE TIME 2 (EMERY: 200px Wide x 228px High)
//   // ---------------------------------------------------------------------------

//   // --- RULE #1: HEADER / STATUS / BATTERY INDICATOR ---
//   // Pins directly against the top bezel of the watch screen
//   #define STATUS_BATTERY_Y_AXIS_TOP             4
//   #define STATUS_BATTERY_WIDTH_PIXELS          20
//   #define STATUS_BATTERY_HEIGHT_PIXELS         10

//   // --- RULE #3: MIDDLE COMPONENT (2D MAP / 3D GLOBE) ---
//   // The vertical bounding band that houses the Earth
//   #define MAP_BAND_Y_AXIS_TOP                 54
//   #define MAP_BAND_HEIGHT_PIXELS              126

//   // Specific geometry for each projection
//   #define GLOBE_3D_DIAMETER_PIXELS            116   // 3D Sphere Diameter (Radius = 58px)
//   #define MAP_2D_WIDTH_PIXELS                 200   // 2D Equirectangular Panoramic Width
//   #define MAP_2D_HEIGHT_PIXELS                100   // 2D Equirectangular Panoramic Height (2:1 Ratio)

//   // --- RULE #2 & #4: ASYMMETRIC POSITIONING BIAS RATIOS (40/60 RULE) ---
//   // When text does NOT overlap, its center sits at 40% distance from the bezel 
//   // and 60% distance from the map edge.
//   #define TOP_TEXT_BEZEL_BIAS_PERCENT          40
//   #define BOTTOM_TEXT_BEZEL_BIAS_PERCENT       40

//   // Max allocated bounding box height for dynamic text measuring
//   #define TOP_TEXT_MAX_BOUNDING_HEIGHT_PIXELS    70
//   #define BOTTOM_TEXT_MAX_BOUNDING_HEIGHT_PIXELS 50

// #else
//   // ---------------------------------------------------------------------------
//   // PLATFORM: PEBBLE TIME / TIME STEEL (BASALT: 144px Wide x 168px High)
//   // ---------------------------------------------------------------------------

//   #define STATUS_BATTERY_Y_AXIS_TOP             3
//   #define STATUS_BATTERY_WIDTH_PIXELS          18
//   #define STATUS_BATTERY_HEIGHT_PIXELS          9

//   #define MAP_BAND_Y_AXIS_TOP                 42
//   #define MAP_BAND_HEIGHT_PIXELS               90

//   #define GLOBE_3D_DIAMETER_PIXELS             80
//   #define MAP_2D_WIDTH_PIXELS                 144
//   #define MAP_2D_HEIGHT_PIXELS                 72

//   #define TOP_TEXT_BEZEL_BIAS_PERCENT          40
//   #define BOTTOM_TEXT_BEZEL_BIAS_PERCENT       40

//   #define TOP_TEXT_MAX_BOUNDING_HEIGHT_PIXELS    50
//   #define BOTTOM_TEXT_MAX_BOUNDING_HEIGHT_PIXELS 40
// #endif



// #pragma once
// #include <pebble.h>

// #if PBL_DISPLAY_HEIGHT >= 200
//   // ================= PEBBLE TIME 2 (EMERY 200x228) =================
//   // Header: Large Time + Flanking Status Icons
//   #define TIME_LAYER_Y         0
//   #define TIME_LAYER_H        46
//   #define STATUS_BATTERY_Y     6
//   #define STATUS_BT_Y          6

//   // Middle: Globe / Map Showcase
//   #define MAP_AREA_Y          46
//   #define MAP_AREA_H         134
//   #define GLOBE_3D_DIAMETER  116    // Radius 58 (Clean 9px breathing room)
//   #define MAP_2D_WIDTH       200    // Edge-to-edge
//   #define MAP_2D_HEIGHT      100    // Exact 2:1 ratio

//   // Footer: Modular Dock (Date now, HR Sparkline + Stats later)
//   #define WIDGET_DOCK_Y      180
//   #define WIDGET_DOCK_H       48

// #else
//   // ================= PEBBLE TIME / STEEL (BASALT 144x168) =================
//   #define TIME_LAYER_Y         0
//   #define TIME_LAYER_H        40
//   #define STATUS_BATTERY_Y     4
//   #define STATUS_BT_Y          4

//   #define MAP_AREA_Y          40
//   #define MAP_AREA_H          88
//   #define GLOBE_3D_DIAMETER   80
//   #define MAP_2D_WIDTH       144
//   #define MAP_2D_HEIGHT       72

//   #define WIDGET_DOCK_Y      128
//   #define WIDGET_DOCK_H       40
// #endif