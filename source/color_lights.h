
#ifndef COLOR_LIGHTS_H_
#define COLOR_LIGHTS_H_

#include "aqualink.h"
#include "aq_programmer.h"

#define LIGHT_COLOR_NAME    16
#define LIGHT_COLOR_OPTIONS 19
//#define LIGHT_DIMER_OPTIONS 4
//#define LIGHT_COLOR_TYPES   LC_DIMMER+1

// The status returned from RS Serial Adapter has this added as a base.  
#define RSSD_COLOR_LIGHT_OFFSET_WRITE 128 // when writing its 128
#define RSSD_COLOR_LIGHT_OFFSET_READ  64 // when reading is 64
#define RSSD_DIMMER_LIGHT_OFFSET 128

// When selecting a color light, the Button press keycode add this offset.
#define IAQ_COLOR_LIGHT_OFFSET  16

//#define DIMMER_LIGHT_TYPE_INDEX 10

/*
// color light modes (Aqualink program, Jandy, Jandy LED, SAm/SAL, Color Logic, Intellibrite)
typedef enum clight_type {
  LC_PROGRAMABLE=0, 
  LC_JANDY, 
  LC_JANDYLED, 
  LC_SAL, 
  LC_CLOGIG, 
  LC_INTELLIB,
} clight_type;
*/
//const char *light_mode_name(clight_type type, int index);
const char *get_currentlight_mode_name(clight_detail light, emulation_type protocol);
const char *light_mode_name(clight_type type, int index, emulation_type protocol);
//int build_color_lights_js(struct aqualinkdata *aqdata, char* buffer, int size);
int build_color_lights_json(struct aqualinkdata *aqdata, char* buffer, int size);
int build_color_light_jsonarray(int index, char* buffer, int size);
int dimmer_percent_to_mode_index(int value);
int dimmer_mode_to_percent(int value);
void clear_aqualinkd_light_modes();
bool set_currentlight_value(clight_detail *light, int index);
bool is_valid_light_mode(clight_type type, int index);
const char* lightTypeName(clight_type type);
int light_mode_index(clight_type type, const char *name);

bool set_aqualinkd_light_mode_name(char *name, int index, bool isShow);
const char *get_aqualinkd_light_mode_name(int index, bool *isShow);

int get_num_light_modes(int index);

//char *_color_light_options_[LIGHT_COLOR_TYPES][LIGHT_COLOR_OPTIONS][LIGHT_COLOR_NAME];

#endif //COLOR_LIGHTS_H_
/*

Rev T.2 has the below
Jandy colors    <- Same
Jandy LED       <- Same
SAm/Sal         <- Same
IntelliBrite    <- Same
Hayw Univ Col   <- Color Logic

*/
/*
Color Name      Jandy Colors    Jandy LED       SAm/SAL         Color Logic     IntelliBrite      dimmer
---------------------------------------------------------------------------------------------------------
Color Splash    11                              8                                              
Alpine White    1               1                                                              
Sky Blue        2               2                                                              
Cobalt Blue     3               3                                                              
Caribbean Blu   4               4                                                              
Spring Green    5               5                                                              
Emerald Green   6               6                                                              
Emerald Rose    7               7                                                              
Magenta         8               8                                                              
Garnet Red      9               9                                                              
Violet          10              10                                                             
Slow Splash                     11                                                             
Fast Splash                     12                                                             
USA!!!                          13                                                             
Fat Tuesday                     14                                                             
Disco Tech                      15                                                             
White                                           1                                              
Light Green                                     2                                              
Green                                           3                                              
Cyan                                            4                                              
Blue                                            5                                              
Lavender                                        6                                              
Magenta                                         7                                              
Light Magenta                                                                                  
Voodoo Lounge                                                   1                              
Deep Blue Sea                                                   2                              
Afternoon Skies                                                 3                              
Afternoon Sky                                                                                 
Emerald                                                         4                              
Sangria                                                         5                              
Cloud White                                                     6                              
Twilight                                                        7                              
Tranquility                                                     8                              
Gemstone                                                        9                             
USA!                                                            10                             
Mardi Gras                                                      11                             
Cool Cabaret                                                    12  
SAm                                                                             1              
Party                                                                           2              
Romance                                                                         3              
Caribbean                                                                       4              
American                                                                        5              
Cal Sunset                                                                      6              
Royal                                                                           7              
Blue                                                                            8              
Green                                                                           9              
Red                                                                             10             
White                                                                           11             
Magenta                                                                         12 
25%                                                                                             1
50%                                                                                             2
75%                                                                                             3
100%                                                                                            4
*/


/*
 * =============================================================================
 *  POOL LIGHT POWER-INTERRUPT TIMING SPECIFICATIONS
 * =============================================================================
 *
 *  Overview:
 *  Multicolor pool lights rely on AC power interrupts (relays) to step their
 *  internal microcontrollers through mode arrays or trigger state resets.
 *  While exact timing parameters vary by manufacturer, all major systems
 *  define specific timing windows for:
 *
 *    - Initial ON (X) : Stabilization/charge time before pulse sequence begins
 *    - Reset OFF  (Y) : Duration off to clear state memory & return to Mode 1
 *    - Pulse (Z)      : ON/OFF toggle interval to step through light modes
 *    - Memory Lock    : Minimum OFF duration required to save active mode
 *
 * =============================================================================
 *  MANUFACTURER TIMING SPECIFICATIONS
 * =============================================================================
 *
 *  1. HAYWARD COLORLOGIC
 *  ---------------------------------------------------------------------------
 *  - Initial ON (X):    15 seconds
 *                       Allows internal power supply capacitors to charge and
 *                       state machine to stabilize.
 *  - Reset OFF (Y):     12 to 15 seconds
 *                       Forces light to clear state memory and reset to
 *                       Mode 1 (Voodoo White).
 *  - Pulse Cycle (Z):   1.0 second ON / 1.0 second OFF
 *                       Toggle power N times to jump directly to mode N.
 *  - Memory Lock:       OFF for > 15 seconds saves active mode to NVM.
 *
 * =============================================================================
 *  2. PENTAIR INTELLIBRITE (5G, MicroBrite, GloBrite)
 *  ---------------------------------------------------------------------------
 *  - Initial ON (X):    10 seconds
 *                       Ensures system is out of previous pulse sequence.
 *  - Reset/Sync OFF(Y): 10 seconds
 *                       Resets state machine to Mode 1 (SAm Mode).
 *  - Pulse Cycle (Z):   0.5 to 1.0 second OFF / 0.5 to 1.0 second ON
 *                       Toggle power N times to jump to mode N:
 *                       (1 = SAm, 2 = Party, 3 = Romance, 5 = American, etc.)
 *  - Memory Lock:       OFF for > 5 seconds saves active mode.
 *
 * =============================================================================
 *  3. JANDY WATERCOLORS LED
 *  ---------------------------------------------------------------------------
 *  - Initial ON (X):    10 seconds
 *  - Reset OFF (Y):     4 to 6 seconds
 *                       Holding OFF specifically for 4-6 seconds forces a hard
 *                       reset to Mode 1 (Alpine White).
 *  - Pulse Cycle (Z):   OFF/ON toggle within < 3.0 seconds total window
 *                       (typically 0.5s OFF / 0.5s ON) increments mode by 1.
 *  - Memory Lock:       OFF for > 7 seconds locks in current mode.
 *
 * =============================================================================
 *  4. PENTAIR SAm / SAL (Legacy Motorized Color Wheel)
 *  ---------------------------------------------------------------------------
 *  - Initial ON (X):    10 seconds
 *  - 1st Pulse:         Toggle OFF/ON within < 3 seconds.
 *                       Fast-forwards mechanical wheel to White, holds for a
 *                       30-second sync pause, then begins color rotation.
 *  - 2nd Pulse:         Toggle OFF/ON within < 3 seconds.
 *                       Locks/holds the color wheel on active color.
 *  - Reset OFF (Y):     OFF for > 10 seconds retains position.
 *
 * =============================================================================
 *  SUMMARY REFERENCE MATRIX
 * =============================================================================
 *
 *  +-----------------------+------------+-------------+-------------------+------------------+
 *  | Manufacturer / Model  | Initial X  | Reset OFF Y | Pulse Timing Z    | Memory Save      |
 *  +-----------------------+------------+-------------+-------------------+------------------+
 *  | Hayward ColorLogic    | 15 sec     | 12s - 15s   | 1.0s ON / 1.0s OFF| OFF > 15 sec     |
 *  | Pentair IntelliBrite  | 10 sec     | 10 sec      | 0.5s - 1.0s ON/OFF| OFF > 5 sec      |
 *  | Jandy WaterColors     | 10 sec     | 4s - 6s     | < 3.0s total      | OFF > 7 sec      |
 *  | Pentair SAm / SAL     | 10 sec     | 10 sec      | < 3.0s toggle     | OFF > 10 sec     |
 *  +-----------------------+------------+-------------+-------------------+------------------+
 *
 * =============================================================================
 */

