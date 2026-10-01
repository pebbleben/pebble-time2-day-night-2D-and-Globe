#pragma once
#include <pebble.h>

#define SETTINGS_KEY 3
#define MAP_PROJECTION_2D 0
#define MAP_PROJECTION_3D 1

typedef struct ClaySettings {
  GColor BackgroundColor;
  GColor ForegroundColor;
  int DayMapIndex;
  int NightMapIndex;
  int LongitudeOffset;
  int LatitudeOffset;
  int CenterFocus;
  GColor DayLand;
  GColor DayWater;
  GColor DayIce;
  GColor NightLand;
  GColor NightWater;
  GColor NightIce;
  char TimeFont[32];
  char DateFont[32];
  int MapProjection;
  // --- New Marker Fields ---
  bool MarkerEnabled;
  int MarkerLat;
  int MarkerLon;
  GColor MarkerColor;
  int AltitudeZoom; // 100 = 1.0x, 140 = 1.4x, 180 = 1.8x
} __attribute__((__packed__)) ClaySettings;

void settings_init(void);
ClaySettings *settings_get(void);
void settings_save(void);
bool settings_update_from_dict(DictionaryIterator *iter);