module.exports = [
  {
    "type": "heading",
    "defaultValue": "World Map Config"
  },
  {
    "type": "section",
    "items": [
      {
        "type": "select",
        "messageKey": "Theme",
        "defaultValue": "classic",
        "label": "Color Theme",
        "options": [
          { "label": "Custom (Manual)", "value": "custom" },
          // High Contrast E-Paper Optimized
          { "label": "Classic Pebble [High Contrast]", "value": "classic" },          
          { "label": "Monochrome [High Contrast]", "value": "bw" },
          { "label": "Vintage Atlas [High Contrast]", "value": "atlas" },
//           { "label": "Sepia Parchment [High Contrast]", "value": "sepia" },           
          { "label": "Red [High Contrast]", "value": "redHC" },
          { "label": "Orange [High Contrast]", "value": "orangeHC" },
          { "label": "Yellow [High Contrast]", "value": "yellowHC" },
          { "label": "Green [High Contrast]", "value": "greenHC" },
          { "label": "Blue [High Contrast]", "value": "blueHC" },                                                 
//           { "label": "Tactical Radar [High Contrast]", "value": "radar" },
          { "label": "Solar Flare [High Contrast]", "value": "solar" },          
          // Medium / Styled Themes
//           { "label": "Monochrome", "value": "bw" },
          { "label": "Retro Terminal", "value": "terminal" },
          { "label": "Ocean Blue", "value": "ocean" },
          { "label": "Arctic Ice", "value": "arctic" },
          { "label": "Aurora", "value": "aurora" },
          { "label": "Deep Jungle", "value": "jungle" },
//           { "label": "Vintage Atlas", "value": "atlas" },
          { "label": "Martian Desert", "value": "mars" },
          { "label": "Midnight Gold", "value": "gold" },
          { "label": "Cherry Cola", "value": "cherry" },
          { "label": "Miami Sunset", "value": "miami" },
          { "label": "Tropical Punch", "value": "tropical" },
          { "label": "Cotton Candy", "value": "candy" },
          { "label": "Neon Cyberpunk", "value": "cyberpunk" },
          { "label": "80s Arcade", "value": "arcade" }
        ]
      },
      { "type": "color", "messageKey": "BackgroundColor", "defaultValue": "0x000000", "label": "Background Color" },
      { "type": "color", "messageKey": "ForegroundColor", "defaultValue": "0xFFFFFF", "label": "Text Color" }
    ]
  },
  {
    "type": "section",
    "items": [
      { "type": "heading", "defaultValue": "Typography" },
      {
        "type": "select",
        "messageKey": "TimeFont",
        "defaultValue": "oswald",
        "label": "Time Font Style",
        "options": [
          { "label": "Barlow Bold (Modern 62)", "value": "barlow" },          
          { "label": "Oswald Bold (Tall 54)", "value": "oswald" },
          { "label": "Retro Digital (Leco 42)", "value": "leco" },
          { "label": "Chunky Bold (Bitham 42)", "value": "bitham" },
          { "label": "Classic Serif (Serif 28)", "value": "serif" }
        ]
      },
      {
        "type": "select",
        "messageKey": "DateFont",
        "defaultValue": "gothic28",
        "label": "Date Font Style",
        "options": [
          { "label": "Chunky Black (Bitham 30)", "value": "bitham" },
          { "label": "Clean Bold (Gothic 28)", "value": "gothic28" },
          { "label": "Classic Serif (Serif 28)", "value": "serif" },
          { "label": "Medium Sans (Gothic 24)", "value": "gothic24" },
          { "label": "Small & Minimal (Gothic 18)", "value": "small" }
        ]
      }
    ]
  },
  //////////////////////////////////////////////////////////////////
    {
    "type": "section",
    "items": [
      { "type": "heading", "defaultValue": "Map Behavior" },
      {
        "type": "select",
        "messageKey": "MapProjection",
        "defaultValue": "0",
        "label": "Earth View",
        "options": [
          { "label": "2D World Map", "value": "0" },
          { "label": "3D Globe", "value": "1" }
        ]
      },
      {
        "type": "select",
        "messageKey": "AltitudeZoom",
        "defaultValue": "100",
        "label": "Globe Zoom",
        "options": [
          { "label": "Whole Globe", "value": "100" },
          { "label": "Sub-Hemisphere", "value": "120" },
          { "label": "Continental", "value": "140" },
          { "label": "Regional", "value": "180" }
        ]
      },
      {
        "type": "select",
        "messageKey": "CenterFocus",
        "defaultValue": "0",
        "label": "Map Focus Mode",
        "options": [          
          { "label": "Fixed Location (Use Sliders)", "value": "0" },
          { "label": "Center on Nightfall", "value": "4" },
          { "label": "Center on Night", "value": "2" },                    
          { "label": "Center on Daybreak", "value": "3" },
          { "label": "Center on Day", "value": "1" }          
        ]
      },
      {
        "type": "slider",
        "messageKey": "LatitudeOffset",
        "defaultValue": 0,
        "label": "View Latitude",
        "description": "Tilt the 3D globe north or south.",
        "min": -90,
        "max": 90,
        "step": 5
      },
      {
        "type": "slider",
        "messageKey": "LongitudeOffset",
        "defaultValue": 0,
        "label": "View Longitude",
        "description": "Center longitude: negative is west, positive is east.",
        "min": -180,
        "max": 180,
        "step": 5
      }
    ]
  },
//   {
//     "type": "section",
//     "items": [
//       { "type": "heading", "defaultValue": "Map Behavior" },
//       {
//         "type": "select",
//         "messageKey": "MapProjection",
//         "defaultValue": "0",
//         "label": "Earth View",
//         "options": [
//           { "label": "2D World Map", "value": "0" },
//           { "label": "3D Globe", "value": "1" }
//         ]
//       },
//       {
//         "type": "select",
//         "messageKey": "CenterFocus",
//         "defaultValue": "0",
//         "label": "Map Focus Mode",
//         "options": [
//           { "label": "Fixed Location (Use Sliders)", "value": "0" },
//           { "label": "Center on Day", "value": "1" },
//           { "label": "Center on Night", "value": "2" }
//         ]
//       },
//       {
//         "type": "slider",
//         "messageKey": "LatitudeOffset",
//         "defaultValue": 0,
//         "label": "View Latitude",
//         "description": "Tilt the 3D globe north or south. Also applies while following day or night.",
//         "min": -90,
//         "max": 90,
//         "step": 5
//       },
//       {
//         "type": "slider",
//         "messageKey": "LongitudeOffset",
//         "defaultValue": 0,
//         "label": "View Longitude",
//         "description": "Center longitude: negative is west, positive is east.",
//         "min": -180,
//         "max": 180,
//         "step": 5
//       }
//     ]
//   },
  ////////////////////////////////////////////////////////////////
    {
    "type": "section",
    "items": [
      { "type": "heading", "defaultValue": "Location Marker" },
      {
        "type": "toggle",
        "messageKey": "MarkerEnabled",
        "defaultValue": false,
        "label": "Show Location Dot"
      },
      {
        "type": "slider",
        "messageKey": "MarkerLat",
        "defaultValue": 40,
        "label": "Marker Latitude",
        "min": -90,
        "max": 90,
        "step": 1
      },
      {
        "type": "slider",
        "messageKey": "MarkerLon",
        "defaultValue": -74,
        "label": "Marker Longitude",
        "min": -180,
        "max": 180,
        "step": 1
      },
      {
        "type": "color",
        "messageKey": "MarkerColor",
        "defaultValue": "0xFF0000",
        "label": "Marker Color"
      }
    ]
  },
  {
    "type": "section",
    "items": [
      { "type": "heading", "defaultValue": "Day Appearance" },
      {
        "type": "select",
        "messageKey": "DayMap",
        "defaultValue": "2",
        "label": "Day Map Style",
        "options": [
          { "label": "Simple and Clean", "value": "0" },
          { "label": "Blue Marble", "value": "1" },
          { "label": "Custom Colors", "value": "2" }
        ]
      },
      { "type": "color", "messageKey": "DayLand", "defaultValue": "0x00FF00", "label": "Day Land" },
      { "type": "color", "messageKey": "DayWater", "defaultValue": "0x0000FF", "label": "Day Water" },
      { "type": "color", "messageKey": "DayIce", "defaultValue": "0xFFFFFF", "label": "Day Ice" }
    ]
  },
  {
    "type": "section",
    "items": [
      { "type": "heading", "defaultValue": "Night Appearance" },
      {
        "type": "select",
        "messageKey": "NightMap",
        "defaultValue": "2",
        "label": "Night Map Style",
        "options": [
          { "label": "Dithered", "value": "0" },
          { "label": "Clean", "value": "1" },
          { "label": "Custom Colors", "value": "2" }
        ]
      },
      { "type": "color", "messageKey": "NightLand", "defaultValue": "0x005500", "label": "Night Land" },
      { "type": "color", "messageKey": "NightWater", "defaultValue": "0x000055", "label": "Night Water" },
      { "type": "color", "messageKey": "NightIce", "defaultValue": "0x555555", "label": "Night Ice" }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];



