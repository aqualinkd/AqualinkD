

#include <stdio.h>
#include <string.h>

//#define COLOR_LIGHTS_C_
#include "color_lights.h"
#include "rs_msg_utils.h"


/*
Jandy Colors
Jandy LED Light
Sam/SL
Color Logic
Intelibright
Haywood Universal Color
*/
bool isShowMode(const char *mode);

/*
Jandy Colors 	
Jandy LED Light 	
SAm/SAL Light 	
IntelliBrite 	
Hayw Univ Color
*/

/****** This list MUST be in order of clight_type enum *******/
char *_color_light_options[NUMBER_LIGHT_COLOR_TYPES][LIGHT_COLOR_OPTIONS] = 
//char *_color_light_options[NUMBER_LIGHT_COLOR_TYPES][LIGHT_COLOR_OPTIONS] = 
{
   // 0 = AqualnkD Colors ignored as no names in control panel.
   { "Off", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18" },
   { // 1 = Jandy Color
        "Off",
        "Alpine White",  // 0x41
        "Sky Blue",
        "Cobalt Blue",
        "Caribbean Blu",
        "Spring Green",
        "Emerald Green",
        "Emerald Rose",
        "Magenta",
        "Garnet Red",
        "Violet",
        "Color Splash"   // 0x4b
  },
  { // 2 = Jandy LED
        "Off",
        "Alpine White",
        "Sky Blue",
        "Cobalt Blue",
        "Caribbean Blu",
        "Spring Green",
        "Emerald Green",
        "Emerald Rose",
        "Magenta",
        "Violet",
        "Slow Splash",
        "Fast Splash",
        "USA",         // America the Beautiful  <- Think this is Infinite Water Colors. Need to check what version that changed.
        "Fat Tuesday",
        "Disco Tech"
  },
  { // 3 = SAm/SAL
        "Off",
        "White",
        "Light Green",
        "Green",
        "Cyan",
        "Blue",
        "Lavender",
        "Magenta"
  },
  { // 4 = Color Logic 
        "Off",
        "Voodoo Lounge",   // 0x41 (both home and sim)
        "Deep Blue Sea",   // 0x42 (both gome and sim)
        "Afternoon Skies", // 0x44 home // 0x43 sim // 'Afternoon Sky' on allbutton, Skies on iaqtouch
        "Emerald",         // 0x44
        "Sangria",
        "Cloud White",     // 0x46
        "Twilight",        // 0x4c (home panel) // 0x47
        "Tranquility",     // 0x4d (home panel) // 0x48
        "Gemstone",        // 0x4e (home panel) // 0x49 (simulator)
        "USA",             // 0x4f (home panel) // 0x4a (simulator)
        "Mardi Gras",      // 0x50 (home panel) // 0x4b (simulator)
        "Cool Cabaret"     // 0x51 (home panel) // 0x4c
  },                 
  { // 5 = IntelliBrite
        "Off",
        "SAm",
        "Party",
        "Romance",
        "Caribbean",
        "American",
        "Cal Sunset",
        "Royal",
        "Blue",
        "Green",
        "Red",
        "White",
        "Magenta"
  },
  { // 6 = Haywood Universal Color
        "Off",
        "Voodoo Lounge",   // 0x41 (both home and sim) // Looks like 28 + <value or index> = 0x41 = 1st index
        "Deep Blue Sea",   // 0x42 (both gome and sim)
        "Royal Blue",    // // 0x43 home // non
        "Afternoon Skies", // 0x44 home // 0x43 sim // 'Afternoon Sky' on allbutton, Skies on iaqtouch
        "Aqua Green",    //  
        "Emerald",         // 0x44
        "Cloud White",     // 0x46
        "Warm Red",      //  
        "Flamingo",      //  
        "Vivid Violet",  //  
        "Sangria",       // 0x4b (home panel) // Non existant
        "Twilight",        // 0x4c (home panel) // 0x47
        "Tranquility",     // 0x4d (home panel) // 0x48
        "Gemstone",        // 0x4e (home panel) // 0x49 (simulator)
        "USA",             // 0x4f (home panel) // 0x4a (simulator)
        "Mardi Gras",      // 0x50 (home panel) // 0x4b (simulator)
        "Cool Cabaret"     // 0x51 (home panel) // 0x4c
  },
  {// 7 = Jandy Infinite Water Colors (RS485)
        "Off",
        "Alpine White",
        "Sky Blue",
        "Cobalt Blue",
        "Caribbean Blue",
        "Spring Green",
        "Emerald Green",
        "Emerald Rose",
        "Ruby Red",    // Added over Jandy LED
        "Magenta",
        "Violet",
        "Slow Splash",
        "Fast Splash",
        "America The Beautiful",         // America the Beautiful  <- Think this is Infinite Water Colors. Need to check what version that changed.
        "Fat Tuesday",
        "Disco Tech"
  },
  {/* 8 = Spare 2*/},
  {/* 9 = Spare 3*/},
  { // 10 = Dimmer    // From manual this is 0 for off, 128+<value%> so 153 = 25%  = 0x99
        "Off",
        "25%",  // 0x99 (simulator)  = 153 dec
        "50%",  // 0xb2 (simulator)  = 178 dec  same as (0x99 + 25)
        "75%",  // 0xcb (simulator)  = 203 dec
        "100%"  // 0xe4              = 228 dec
  },
  {/* 11 = Dimmer with full range */}
};

// DON'T FORGET TO CHANGE    #define DIMMER_LIGHT_INDEX 10   in color_lights.h



/*
void deleteLightOption(int type, int index) 
{
  int arrlen = LIGHT_COLOR_OPTIONS;
  memmove(_color_light_options[type]+index, _color_light_options[type]+index+1, (--arrlen - index) * sizeof *_color_light_options[type]);
}

void setColorLightsPanelVersion(uint8_t supported)
{
  static bool set = false;
  if (set)
    return;

  if ((supported & REP_SUP_CLIT4) == REP_SUP_CLIT4)
    return; // Full panel support, no need to delete anything

  //deleteLightOption(4, 11); // Color Logic "Sangria"
  deleteLightOption(4, 9); // Color Logic "Vivid Violet",
  deleteLightOption(4, 8);  // Color Logic "Flamingo"
  deleteLightOption(4, 7);  // Color Logic "Warm Red",
  deleteLightOption(4, 4);  // Color Logic "Aqua Green"
  deleteLightOption(4, 2);  // Color Logic "Royal Blue"
  
  set = true;
}
*/

bool is_valid_light_mode(clight_type type, int index)
{
  if (type == LC_DIMMER2) {
    if (index >= 0 && index <=100) {
      return true;
    } else {
      return false;
    }
  }
  
  if (index < 0 || index > LIGHT_COLOR_OPTIONS || _color_light_options[type][index] == NULL ){
    return false;
  }

  return true;
}

void clear_aqualinkd_light_modes()
{
  //_color_light_options[0] = _aqualinkd_custom_colors;

  for (int i=0; i < LIGHT_COLOR_OPTIONS; i++) {
    _color_light_options[0][i] = NULL;
    //_color_light_options[0][i] = i;
    //_color_light_options[0][i] = _aqualinkd_custom_colors[i];
  }
}

int get_num_light_modes(int index)
{
  int length;

  for (length=0; length < LIGHT_COLOR_OPTIONS; length++) {
    if (_color_light_options[index][length] == NULL) {
      break;
    }
  }

  return length--; // index 0 is off, so don;t count that 
}

bool set_aqualinkd_light_mode_name(char *name, int index, bool isShow)
{
  static bool reset = false;

  // Reset all options only once.
  if (!reset) {
    reset = true;
    for (int i=1; i<LIGHT_COLOR_OPTIONS; i++) {
      _color_light_options[0][i] = NULL;
    }
  }

  // TODO NSF check isShow and add a custom one if needed
  _color_light_options[0][index] = name;

  return true;
}

const char *get_aqualinkd_light_mode_name(int index, bool *isShow)
{
  // if index 1 is "1" then none are set.
  if ( _color_light_options[0][1] == NULL || strcmp(_color_light_options[0][1], "1") == 0) {
    return NULL;
  }

  *isShow = isShowMode(_color_light_options[0][index]);
  return _color_light_options[0][index];
}

const char *get_currentlight_mode_name(clight_detail light, emulation_type protocol) 
{
  /*
  if (light.lightType == LC_PROGRAMABLE && light.button->led->state == OFF) {
    return "Off";
  }
  */

  // Programmable light that's on but no mode, just return blank
  if (light.lightType == LC_PROGRAMABLE && light.button->led->state == ON && light.currentValue == 0) {
    return "";
  }

  if (light.currentValue < 0 || light.currentValue > LIGHT_COLOR_OPTIONS ){
    return "";
  } 

  if (_color_light_options[light.lightType][light.currentValue] == NULL) {
    return "";
  }
  // Rename any modes depending on emulation type
  if (protocol == ALLBUTTON) {
    // Afternoon Skies = Afternoon Sky
    if (strcmp(_color_light_options[light.lightType][light.currentValue],"Afternoon Skies") == 0) {
      return "Afternoon Sky";
    }
    //Caribbean Blue = Caribbean Blu
    if (strcmp(_color_light_options[light.lightType][light.currentValue],"Caribbean Blue") == 0) {
      return "Caribbean Blu";
    }
    
  }

  return _color_light_options[light.lightType][light.currentValue];
}



// This should not be uses for getting current lightmode name since it doesn;t have full logic

const char *light_mode_name(clight_type type, int index, emulation_type protocol)
{
  if (index < 0 || index > LIGHT_COLOR_OPTIONS ){
    return "";
  } 

  if (_color_light_options[type][index] == NULL) {
    return "";
  }

  // Rename any modes depending on emulation type
  if (protocol == ALLBUTTON) {
    if (strcmp(_color_light_options[type][index],"Afternoon Skies") == 0) {
      return "Afternoon Sky";
    }
  }

  return _color_light_options[type][index];
}

int light_mode_index(clight_type type, const char *name)
{
  for (int i=0; i < LIGHT_COLOR_OPTIONS; i++) {
     if (_color_light_options[type][i] == NULL) {
      return AQ_UNKNOWN;
     }

     if ( rsm_strmatch(_color_light_options[type][i], name) == 0) {
      return i;
     } else if (rsm_strmatch_ignore(name, _color_light_options[type][i], -1) == 0) { // Remove 1 char from check for USA! to USA.
       return i;
     }
  }

  return AQ_UNKNOWN;
}


bool isShowMode(const char *mode)
{
  if (mode == NULL)
    return false;
    
  if (strcmp(mode, "Color Splash") == 0 ||
      strcmp(mode, "Slow Splash") == 0 ||
      strcmp(mode, "Fast Splash") == 0 ||
      strcmp(mode, "Fat Tuesday") == 0 ||
      strcmp(mode, "Disco Tech") == 0 ||
      strcmp(mode, "Voodoo Lounge") == 0 ||
      strcmp(mode, "Twilight") == 0 ||
      strcmp(mode, "Tranquility") == 0 ||
      strcmp(mode, "Gemstone") == 0 ||
      strcmp(mode, "USA") == 0 ||
      strcmp(mode, "Mardi Gras") == 0 ||
      strcmp(mode, "Cool Cabaret") == 0 ||
      strcmp(mode, "SAm") == 0 ||
      strcmp(mode, "Party") == 0 ||
      strcmp(mode, "Romance") == 0 ||
      strcmp(mode, "Caribbean") == 0 ||
      strcmp(mode, "American") == 0 ||
      strcmp(mode, "Cal Sunset") == 0)
    return true;
  else
    return false;
}


// NSF need to use the SET_IF_CHANGED macro here, so come back and change function parameters.
bool set_currentlight_value(clight_detail *light, int index)
{
  bool rtn=false;
  // Dimmer 2 has different values (range 1 to 100)
  if (light->lightType == LC_DIMMER2) {
    if (index < 0 || index > 100)
      SET_IF_CHANGED(light->currentValue, 0, rtn);
    else
      SET_IF_CHANGED(light->currentValue, index, rtn);
  } else {
  // We want to leave the last color, so if 0 don't do anything, but set to 0 if bad value
    if (index <= 0 || index > LIGHT_COLOR_OPTIONS) {
      SET_IF_CHANGED(light->currentValue, 0, rtn);
    } else if (index > 0 && index < LIGHT_COLOR_OPTIONS) {
      SET_IF_CHANGED(light->currentValue, index, rtn);
      //light->lastValue = index;
    }
  }

  return rtn;
}

 // Take a % and return 0,1,2,3,4 for 0=0, 1=25, 2=50, 3=75, 4=100, ie the index of _color_light_options[10][??]
int dimmer_percent_to_mode_index(int value)
{
  /*
  value = round(value / 25);
  if (value > 4 ) {value=4;}
  */
  value = round( (value+12) / 25);  // Round up/down 
  if (value > 4 ) {value=4;}

  return value;
}

int dimmer_mode_to_percent(int value)
{
  return value * 25;
}


const char* lightTypeName(clight_type type)
{
  switch (type) {
    case LC_PROGRAMABLE:
      return "AqualinkD Programmable";
    break; 
    case LC_JANDY:
      return "Jandy Color";
    break; 
    case LC_JANDYLED:
      return "Jandy LED";
    break; 
    case LC_SAL:
      return "SAm/SAL";
    break; 
    case LC_CLOGIG:
      return "Color Logic";
    break; 
    case LC_INTELLIB:
      return "Intelibrite";
    break; 
    case LC_HAYWCL:
      return "Haywood Universal";
    break; 
    case LC_JANDYINFINATE:
      return "Jandy Infinite";
    break; 
    case LC_DIMMER:
      return "Dimmer";
    break; 
    case LC_DIMMER2:
      return "Dimmer (full range)";
    break; 

    default:
    case LC_SPARE_2:
    case LC_SPARE_3:
    return "unknown";
    break;
  }
  return "unknown";
}

/*
// Used for dynamic config JS 
int build_color_lights_js(struct aqualinkdata *aqdata, char* buffer, int size)
{
  memset(&buffer[0], 0, size);
  int length = 0;
  int i, j;

  length += sprintf(buffer+length, "var _light_program = [];\n");

  if ( _color_light_options[0][1] == NULL || strcmp(_color_light_options[0][1], "1") == 0) {
    length += sprintf(buffer+length, "_light_program[0] = [];\n");
    i=1;
  } else {
    i=0;
  }
   
  for (; i < NUMBER_LIGHT_COLOR_TYPES; i++) {
    length += sprintf(buffer+length, "_light_program[%d] = [ ", i);
    for (j=1; j < LIGHT_COLOR_OPTIONS; j++) { // Start a 1 since index 0 is blank
      if (_color_light_options[i][j] != NULL)
        length += sprintf(buffer+length, "\"%s%s\",", _color_light_options[i][j], (isShowMode(_color_light_options[i][j])?" - Show":"") );
    }
    buffer[--length] = '\0';
    length += sprintf(buffer+length, "];\n");
  }

  return length;
}
*/
// Used for dynamic config JSON
int build_color_lights_json(struct aqualinkdata *aqdata, char* buffer, int size)
{
  memset(&buffer[0], 0, size);
  int length = 0;
  int i, j;

  length += sprintf(buffer+length, "\"light_programs\": {");

  if ( _color_light_options[0][1] == NULL || strcmp(_color_light_options[0][1], "1") == 0) {
    length += sprintf(buffer+length, "\"0\": [],");
    i=1;
  } else {
    i=0;
  }
   
  for (; i < NUMBER_LIGHT_COLOR_TYPES; i++) {
    length += sprintf(buffer+length, "\"%d\": [", i);
    for (j=1; j < LIGHT_COLOR_OPTIONS; j++) { // Start a 1 since index 0 is blank
      if (_color_light_options[i][j] != NULL)
        length += sprintf(buffer+length, "\"%s%s\"%s", _color_light_options[i][j], (isShowMode(_color_light_options[i][j])?" - Show":""), (_color_light_options[i][j+1] != NULL)?",":"" );
    }
    //buffer[--length] = '\0';
    length += sprintf(buffer+length, "],");
  }

  buffer[--length] = '\0';
  length += sprintf(buffer+length, "}");

  length += sprintf(buffer+length, ",\"light_types\": {");
  for (i=0; i < NUMBER_LIGHT_COLOR_TYPES; i++) {
    length += sprintf(buffer+length, "\"%d\": \"%s\",", i, lightTypeName(i));
  }
  buffer[--length] = '\0';
  length += sprintf(buffer+length, "}");

  return length;
}

int build_color_light_jsonarray(int index, char* buffer, int size)
{
  memset(&buffer[0], 0, size);
  int i;
  int length=0;

  for (i=0; i < LIGHT_COLOR_OPTIONS; i++) { // Start a 1 since index 0 is blank
    if (_color_light_options[index][i] != NULL) {
      length += sprintf(buffer+length, "\"%s\",", _color_light_options[index][i] );
    }
  }
  buffer[--length] = '\0';

  return length;
}

