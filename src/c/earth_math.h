#pragma once
#include <pebble.h>
#include <stdbool.h>
#include <time.h>

#define FP_SCALE 16384 // Q14 fixed point (1.0 = 16384)

typedef struct {
  int32_t x, y, z;
} EarthVector;

typedef struct {
  EarthVector right, up, forward;
} EarthCamera;

// Fast integer square root
static inline int32_t earth_int_sqrt(int32_t val) {
  if (val <= 0) return 0;
  int32_t res = 0;
  int32_t bit = 1 << 30;
  while (bit > val) bit >>= 2;
  while (bit != 0) {
    if (val >= res + bit) {
      val -= res + bit;
      res = (res >> 1) + bit;
    } else {
      res >>= 1;
    }
    bit >>= 2;
  }
  return res;
}

// Keep longitude strictly in [-180, 180)
static inline int32_t earth_wrap_deg(int32_t lon) {
  while (lon >= 180) lon -= 360;
  while (lon < -180) lon += 360;
  return lon;
}

// Fixed-point dot product
static inline int32_t earth_dot(EarthVector a, EarthVector b) {
  return (a.x * b.x + a.y * b.y + a.z * b.z);
}

// 3D Orthographic Camera Matrix in Fixed Point
static inline EarthCamera earth_camera(int16_t lat_deg, int16_t lon_deg) {
  int32_t lat_angle = ((int32_t)lat_deg * TRIG_MAX_ANGLE) / 360;
  int32_t lon_angle = ((int32_t)lon_deg * TRIG_MAX_ANGLE) / 360;

  int32_t sl = (int32_t)sin_lookup(lat_angle) / 4;
  int32_t cl = (int32_t)cos_lookup(lat_angle) / 4;
  int32_t so = (int32_t)sin_lookup(lon_angle) / 4;
  int32_t co = (int32_t)cos_lookup(lon_angle) / 4;

  EarthCamera c;
  // Right: [-sin(lon), 0, cos(lon)]
  c.right.x = -so;
  c.right.y = 0;
  c.right.z = co;

  // Up: [-sin(lat)*cos(lon), cos(lat), -sin(lat)*sin(lon)]
  c.up.x = (-sl * co) / FP_SCALE;
  c.up.y = cl;
  c.up.z = (-sl * so) / FP_SCALE;

  // Forward: [cos(lat)*cos(lon), sin(lat), cos(lat)*sin(lon)]
  c.forward.x = (cl * co) / FP_SCALE;
  c.forward.y = sl;
  c.forward.z = (cl * so) / FP_SCALE;

  return c;
}

// Calculate the Sun vector in Earth Space using UTC
static inline EarthVector earth_sun(time_t now, int32_t *out_sun_lat, int32_t *out_sun_lon) {
  struct tm *utc = gmtime(&now);
  if (!utc) {
    if (out_sun_lat) *out_sun_lat = 0;
    if (out_sun_lon) *out_sun_lon = 0;
    return (EarthVector){FP_SCALE, 0, 0};
  }

  int32_t utc_minutes = utc->tm_hour * 60 + utc->tm_min;
  int32_t day_of_year = utc->tm_yday;
  int32_t year_angle = ((day_of_year + 10) * TRIG_MAX_ANGLE) / 365;

  // Equation of Time (orbital eccentricity adjustment in minutes)
  int32_t eot_angle1 = (day_of_year * TRIG_MAX_ANGLE) / 365;
  int32_t eot_angle2 = (day_of_year * 2 * TRIG_MAX_ANGLE) / 365;
  int32_t eq_minutes = (-cos_lookup(eot_angle1) * 7 / TRIG_MAX_RATIO) + 
                       (sin_lookup(eot_angle2) * 10 / TRIG_MAX_RATIO);

  // Solar longitude (Sun moves Westward: at 12:00 UTC = 0°, at 18:00 UTC = -90° [West])
  int32_t net_solar_minutes = (720 - utc_minutes) - eq_minutes;
  int32_t sun_lon_angle = (net_solar_minutes * TRIG_MAX_ANGLE) / 1440;

  // Solar declination (-23.44° to +23.44°)
  int32_t decl_ratio = -cos_lookup(year_angle);
  int32_t decl_angle = (decl_ratio * 4267) / TRIG_MAX_RATIO;

  if (out_sun_lat) *out_sun_lat = (decl_angle * 360) / TRIG_MAX_ANGLE;
  if (out_sun_lon) *out_sun_lon = earth_wrap_deg((net_solar_minutes * 360) / 1440);

  int32_t sd = (int32_t)sin_lookup(decl_angle) / 4;
  int32_t cd = (int32_t)cos_lookup(decl_angle) / 4;
  int32_t so = (int32_t)sin_lookup(sun_lon_angle) / 4;
  int32_t co = (int32_t)cos_lookup(sun_lon_angle) / 4;

  EarthVector sun;
  sun.x = (cd * co) / FP_SCALE;
  sun.y = sd;
  sun.z = (cd * so) / FP_SCALE;
  return sun;
}