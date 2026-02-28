/*
 * Copyright (c) 2017 Shaun Feakes - All rights reserved
 *
 * You may use redistribute and/or modify this code under the terms of
 * the GNU General Public License version 2 as published by the 
 * Free Software Foundation. For the terms of this license, 
 * see <http://www.gnu.org/licenses/>.
 *
 * You are free to use this software under the terms of the GNU General
 * Public License, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 *  https://github.com/sfeakes/aqualinkd
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "aq_serial.h"
#include "aqualink.h"
#include "utils.h"
#include "packetLogger.h"
#include "iaqtouch.h"
#include "iaqtouch_aq_programmer.h"
#include "iaqualink.h"
#include "aq_programmer.h"
#include "rs_msg_utils.h"
#include "devices_jandy.h"


#define NEW_POLL_CYCLE

void temp_debugprintExtraInfo(unsigned char *pk, int length);


#ifdef ATOUCH_TEST
void set_iaq_cansend(bool yes) {}
bool in_iaqt_programming_mode(struct aqualinkdata *aqdata) {return false;}
bool iaqt_queue_cmd(unsigned char cmd) {}
bool in_programming_mode(struct aqualinkdata *aqdata){return false;}
void queueGetProgramData(emulation_type source_type, struct aqualinkdata *aqdata){}
void kick_aq_program_thread(struct aqualinkdata *aqdata, emulation_type source_type){}
void aq_programmer(program_type type, char *args, struct aqualinkdata *aqdata){}
#endif

unsigned char _button_keys[] = { KEY_IAQTCH_KEY01,
                            KEY_IAQTCH_KEY02,
                            KEY_IAQTCH_KEY03,
                            KEY_IAQTCH_KEY04,
                            KEY_IAQTCH_KEY05,
                            KEY_IAQTCH_KEY06,
                            KEY_IAQTCH_KEY07,
                            KEY_IAQTCH_KEY08,
                            KEY_IAQTCH_KEY09,
                            KEY_IAQTCH_KEY10,
                            KEY_IAQTCH_KEY11,
                            KEY_IAQTCH_KEY12,
                            KEY_IAQTCH_KEY13,
                            KEY_IAQTCH_KEY14,
                            KEY_IAQTCH_KEY15};


#define IAQ_STATUS_PAGE_LINES 18
#define IAQ_PAGE_BUTTONS      24
#define IAQ_MSG_TABLE_LINES   IAQ_STATUS_PAGE_LINES // No idea actual size, so just use this until figured out.
#define IAQT_TABLE_MSGLEN     32

unsigned char _currentPageLoading;
unsigned char _currentPage;

unsigned char _lastMsgType = 0x00;
//unsigned char _last_kick_type = -1;

static char _popupMsg[AQ_MSGLONGLEN + 1];

int _deviceStatusLines = 0;
char _homeStatus[IAQ_STATUS_PAGE_LINES][AQ_MSGLEN+1];
char _deviceStatus[IAQ_STATUS_PAGE_LINES][AQ_MSGLEN+1];
char _tableInformation[IAQ_MSG_TABLE_LINES][IAQT_TABLE_MSGLEN+1];
struct iaqt_page_button _pageButtons[IAQ_PAGE_BUTTONS];
struct iaqt_page_button _homeButtons[IAQ_PAGE_BUTTONS];

// Need to cache these pages, as only get updates after initial load.
struct iaqt_page_button _devicePageButtons[3][IAQ_PAGE_BUTTONS];
struct iaqt_page_button _deviceSystemSetupButtons[3][IAQ_PAGE_BUTTONS];

unsigned char iaqtLastMsg()
{
  return _lastMsgType;
}

const char *iaqtPopupMsg()
{
  return _popupMsg;
}

void set_iaqtouch_lastmsg(unsigned char msgtype)
{
  _lastMsgType = msgtype;
}

bool wasiaqtThreadKickTypePage()
{
  switch(_lastMsgType) {
    //case CMD_IAQ_PAGE_MSG:
    //case CMD_IAQ_PAGE_BUTTON:
    //case CMD_IAQ_PAGE_START:
    case CMD_IAQ_PAGE_END:
      return true;
    break;
    default:
      return false;
    break;
  }

  return false;
}


unsigned char iaqtCurrentPage()
{
  return _currentPage;
}
unsigned char iaqtCurrentPageLoading()
{
  return _currentPageLoading;
}


const char *iaqtGetMessageLine(int index) {
  if (index < IAQ_STATUS_PAGE_LINES)
    return _deviceStatus[index];

  return NULL;
}
const char *iaqtGetTableInfoLine(int index) {
  if (index < IAQ_MSG_TABLE_LINES)
    return _tableInformation[index];

  return NULL;
}

struct iaqt_page_button *iaqtFindButtonByIndex(int index) {
  //int i;
  struct iaqt_page_button *buttons;

  // NSF Need to merge this from iaqtFindButtonByLabel function
  if (_currentPage == IAQ_PAGE_DEVICES || _currentPage == IAQ_PAGE_DEVICES_REV_Yg)
    buttons = _devicePageButtons[0];
  else if (_currentPage == IAQ_PAGE_DEVICES2)
    buttons = _devicePageButtons[1];
  else if (_currentPage == IAQ_PAGE_DEVICES3)
    buttons = _devicePageButtons[2];
  else if (_currentPage == IAQ_PAGE_SYSTEM_SETUP )
    buttons = _deviceSystemSetupButtons[0];
  else if (_currentPage == IAQ_PAGE_SYSTEM_SETUP2 )
    buttons = _deviceSystemSetupButtons[1];
  else if (_currentPage == IAQ_PAGE_SYSTEM_SETUP3 )
    buttons = _deviceSystemSetupButtons[2];
  else if (_currentPage == IAQ_PAGE_HOME )
    buttons = _homeButtons;
  else
    buttons = _pageButtons;

  if (index>=0 && index < IAQ_PAGE_BUTTONS) {
      return &buttons[index];
  }

  return NULL;
}

struct iaqt_page_button *iaqtFindButtonByLabel(const char *label) {
  int i;
  struct iaqt_page_button *buttons;

  if (_currentPage == IAQ_PAGE_DEVICES || _currentPage == IAQ_PAGE_DEVICES_REV_Yg)
    buttons = _devicePageButtons[0];
  else if (_currentPage == IAQ_PAGE_DEVICES2)
    buttons = _devicePageButtons[1];
  else if (_currentPage == IAQ_PAGE_DEVICES3)
    buttons = _devicePageButtons[2];
  else if (_currentPage == IAQ_PAGE_SYSTEM_SETUP )
    buttons = _deviceSystemSetupButtons[0];
  else if (_currentPage == IAQ_PAGE_SYSTEM_SETUP2 )
    buttons = _deviceSystemSetupButtons[1];
  else if (_currentPage == IAQ_PAGE_SYSTEM_SETUP3 )
    buttons = _deviceSystemSetupButtons[2];
  else if (_currentPage == IAQ_PAGE_HOME )
    buttons = _homeButtons;
  else
    buttons = _pageButtons;

  for (i=0; i < IAQ_PAGE_BUTTONS; i++) {
    //if (_pageButtons[i].state != 0 || _pageButtons[i].type != 0 || _pageButtons[i].unknownByte != 0)
    if (rsm_strcmp((char *)buttons[i].name,label) == 0) {
      LOG(IAQT_LOG, LOG_DEBUG, "Found button '%s'\n",label);
      return &buttons[i];
    }
  }

  LOG(IAQT_LOG, LOG_DEBUG, "Did not find button '%s'\n",label);
  return NULL;
}

int num2iaqtRSset (unsigned char* packetbuffer, int num, bool pad4unknownreason)
{
  //unsigned int score = 42;   // Works for score in [0, UINT_MAX]
  //unsigned char tmp;
  //printf ("num via printf:     %u\n", num);   // For validation

  int bcnt = 0;
  int digits = 0;
  unsigned int div = 1;
  unsigned int digit_count = 1;
  while ( div <= num / 10 ) {
    digit_count++;
    div *= 10;
  }
  while ( digit_count > 0 ) {
    packetbuffer[bcnt] = (num / div + 0x30); // 48 = 0x30 base number for some reason. (ie 48=0)
    num %= div;
    div /= 10;
    digit_count--;
    bcnt++;
  }

  for (digits = bcnt; bcnt < 6; bcnt++) { // Note setting digits to bcnt is correct.  Saving current count to different int
    if (bcnt == 4 && digits <= 3 ) // Less than 4 digits (<1000), need to add a 0x30
      packetbuffer[bcnt] = 0x30;
    else
      packetbuffer[bcnt] = NUL;
  }

  return bcnt;
}

int char2iaqtRSset(unsigned char* packetbuffer, char *msg, int msg_len)
{
  int bcnt=0;

  for (bcnt=0; bcnt < msg_len; bcnt++ ){
    packetbuffer[bcnt] = msg[bcnt];
  }

  packetbuffer[bcnt] = 0x00;

  return ++bcnt;
}

/*
void createDeviceUpdatePacket() {
  unsigned char packets[AQ_MAXPKTLEN_SEND];
  int cnt;

  packets[0] = DEV_MASTER;
  packets[1] = 0x24;
  packets[2] = 0x31;

  cnt = num2iaqtRSset(&packets[3], 1000, true);

  for(cnt = cnt+3; cnt <= 18; cnt++)
    packets[cnt] = 0xcd;

  //printHex(packets, 19);
  //printf("\n");

  //send_jandy_command(NULL, packets, cnt);
}
*/

void processPageMessage(unsigned char *message, int length)
{
  if ( (int)message[PKT_IAQT_MSGINDX] >= IAQ_STATUS_PAGE_LINES ) {
    LOG(IAQT_LOG,LOG_ERR, "Run out of IAQT message buffer, need %d have %d\n",(int)message[PKT_IAQT_MSGINDX],IAQ_STATUS_PAGE_LINES);
    return;
  }
  
  if (_currentPageLoading == IAQ_PAGE_HOME || _currentPage == IAQ_PAGE_HOME) {
    rsm_strncpy(_homeStatus[(int)message[PKT_IAQT_MSGINDX]], &message[PKT_IAQT_MSGDATA], AQ_MSGLEN, length-PKT_IAQT_MSGDATA-3);
  } else if (_currentPageLoading == IAQ_PAGE_STATUS || _currentPage == IAQ_PAGE_STATUS) { // 2nd page of device status doesn't give us new page message
    //sprintf(_deviceStatus[(int)message[4]], message[5], AQ_MSGLEN);
    //strncpy(_deviceStatus[(int)message[PKT_IAQT_MSGINDX]], (char *)message + PKT_IAQT_MSGDATA, AQ_MSGLEN);
    rsm_strncpy(_deviceStatus[(int)message[PKT_IAQT_MSGINDX]], &message[PKT_IAQT_MSGDATA], AQ_MSGLEN, length-PKT_IAQT_MSGDATA-3);
  } else {
    //strncpy(_deviceStatus[(int)message[PKT_IAQT_MSGINDX]], (char *)message + PKT_IAQT_MSGDATA, AQ_MSGLEN);
    rsm_strncpy(_deviceStatus[(int)message[PKT_IAQT_MSGINDX]], &message[PKT_IAQT_MSGDATA], AQ_MSGLEN, length-PKT_IAQT_MSGDATA-3);
    //LOG(IAQT_LOG,LOG_ERR, "Request to assign message to unknown page,'%.*s'\n",AQ_MSGLEN,(char *)message + PKT_IAQT_MSGDATA);
  }

  //LOG(IAQT_LOG,LOG_DEBUG, "Message :- '%d' '%.*s'\n",(int)message[PKT_IAQT_MSGINDX], length-PKT_IAQT_MSGDATA-3, &message[PKT_IAQT_MSGDATA]);
}

void processTableMessage(unsigned char *message, int length)
{
  if ( (int)message[5] < IAQ_MSG_TABLE_LINES )
    rsm_strncpy(_tableInformation[(int)message[5]], &message[6], IAQT_TABLE_MSGLEN, length-PKT_IAQT_MSGDATA-3);
  else
    LOG(IAQT_LOG,LOG_ERR, "Run out of IAQT table buffer, need %d have %d\n",(int)message[5],IAQ_MSG_TABLE_LINES);
}

int matchLabel2Button(const char* pageButtonName, aqkey *aqbutton, int ignorechars)
{
  int rtn = rsm_strmatch_ignore(pageButtonName, aqbutton->label, ignorechars);

  if (rtn == 0 && isVBUTTON_ALTLABEL(aqbutton->special_mask) ) {
    ((altlabel_detail *)aqbutton->special_mask_ptr)->in_alt_mode = false;
    LOG(IAQT_LOG,LOG_DEBUG, "Virtual Button `%s` is NOT in alternate state of `%s`\n", aqbutton->label, ((altlabel_detail *)aqbutton->special_mask_ptr)->altlabel);
  } else if (rtn != 0 && isVBUTTON_ALTLABEL(aqbutton->special_mask) ) {
    rtn = rsm_strmatch_ignore(pageButtonName, ((altlabel_detail *)aqbutton->special_mask_ptr)->altlabel,ignorechars );
    if (rtn == 0 ) {
      ((altlabel_detail *)aqbutton->special_mask_ptr)->in_alt_mode = true;
      LOG(IAQT_LOG,LOG_DEBUG, "Virtual Button `%s` is in alternate state of `%s`\n", aqbutton->label, ((altlabel_detail *)aqbutton->special_mask_ptr)->altlabel);
    }
  }

  return rtn;
}

//  aqualinkd button found and updated, AQstart & AQend are index of aqualinkd button array
void updateAQButtonFromPageButton(struct aqualinkdata *aqdata, struct iaqt_page_button *pageButton, int AQstartIndex, int AQendIndex)
{

  for (int i = AQstartIndex; i < AQendIndex; i++)
  {
    //LOG(IAQT_LOG,LOG_DEBUG, "Button compare '%s' to '%s'\n",pageButton->name, aqdata->aqbuttons[i].label);

    int rtn = -1;
    // If we are loading HOME page then simply button name is the label ie "Aux3"
    // If loading DEVICES? page then button name + status is "Aux3 OFF "
    /*
    if (_currentPageLoading == IAQ_PAGE_HOME) {
      rtn = rsm_strmatch((const char *)pageButton->name, aqdata->aqbuttons[i].label);
      if (rtn != 0 && isVBUTTON_ALTLABEL(aqdata->aqbuttons) ) {
        rtn = rsm_strmatch((const char *)pageButton->name, ((vbutton_detail *)aqdata->aqbuttons[i].special_mask_ptr)->altlabel );
        if (rtn = 0 ) {
          ((vbutton_detail *)aqdata->aqbuttons[i].special_mask_ptr)->in_alt_mode = true;
        }
      }
    } else {
      rtn = rsm_strmatch_ignore((const char *)pageButton->name, aqdata->aqbuttons[i].label, 5); // 5 = 3 chars and 2 spaces ' OFF '
    }*/
    if (_currentPageLoading == IAQ_PAGE_HOME) {
      rtn = matchLabel2Button((const char *)pageButton->name, &aqdata->aqbuttons[i], 0);
    } else {
      rtn = matchLabel2Button((const char *)pageButton->name, &aqdata->aqbuttons[i], 5); // 5 = 3 chars and 2 spaces ' OFF '
    }

      if (rtn == 0)
      {
        LOG(IAQT_LOG,LOG_DEBUG, "Found Status for %s state 0x%02hhx\n", aqdata->aqbuttons[i].label, pageButton->state);
        switch(pageButton->state) {
          case 0x00:
              SET_IF_CHANGED(aqdata->aqbuttons[i].led->state, OFF, aqdata->is_dirty);
          break;
          case 0x01:
              SET_IF_CHANGED(aqdata->aqbuttons[i].led->state, ON, aqdata->is_dirty);
              //NSF might want to check virtual light here, if so remove the color mode name, since
              // we didnt turn the light on, we cant state the color mode
          break;
          case 0x02:
              SET_IF_CHANGED(aqdata->aqbuttons[i].led->state, FLASH, aqdata->is_dirty);
          break;
          case 0x03:
              SET_IF_CHANGED(aqdata->aqbuttons[i].led->state, ENABLE, aqdata->is_dirty);
          break;
          default:
            // Dimmer light will have the % as the state. so 0x32=50% 0x4B=75%
            // So anything greater than 0 use as on
            if (pageButton->state > 0x00) {
              SET_IF_CHANGED(aqdata->aqbuttons[i].led->state, ON, aqdata->is_dirty);
            }
            //LOG(IAQT_LOG,LOG_NOTICE, "Unknown state 0x%02hhx for button %s\n",pageButton->state,pageButton->name);
          break;
        }
      
    }
  }
}

void processPageButton(unsigned char *message, int length, struct aqualinkdata *aqdata)
{
  struct iaqt_page_button *button;
  int index = (int)message[PKT_IAQT_BUTINDX];

  if (_currentPageLoading == IAQ_PAGE_DEVICES || _currentPageLoading == IAQ_PAGE_DEVICES_REV_Yg)
    button = &_devicePageButtons[0][index];
  else if (_currentPageLoading == IAQ_PAGE_DEVICES2)
    button = &_devicePageButtons[1][index];
  else if (_currentPageLoading == IAQ_PAGE_DEVICES3)
    button = &_devicePageButtons[2][index];
  else if (_currentPageLoading == IAQ_PAGE_SYSTEM_SETUP)
    button = &_deviceSystemSetupButtons[0][index];
  else if (_currentPageLoading == IAQ_PAGE_SYSTEM_SETUP2)
    button = &_deviceSystemSetupButtons[1][index];
  else if (_currentPageLoading == IAQ_PAGE_SYSTEM_SETUP3)
    button = &_deviceSystemSetupButtons[2][index];
  else if (_currentPageLoading == IAQ_PAGE_HOME )
    button = &_homeButtons[index];
  else {
    button = &_pageButtons[index];
    // if _currentPageLoading = 0x00 then we should use current page
    LOG(IAQT_LOG,LOG_INFO, "Not sure where to add Button %d %s - LoadingPage = %s\n",index,button->name,iaqt_page_name(_currentPageLoading));
  }

  button->state  = message[PKT_IAQT_BUTSTATE];
  button->type  = message[PKT_IAQT_BUTTYPE];
  button->unknownByte = message[PKT_IAQT_BUTUNKNOWN];

  if (message[PKT_IAQT_BUTSTATE] == 0x0d)
    button->keycode = message[PKT_IAQT_BUTTYPE];
  else if (index < 15) {
    button->keycode = _button_keys[index];
  }
  // This doesn't work with return which is 0x00
  
  //strncpy(&button->name, (char *)message + PKT_IAQT_BUTDATA, AQ_MSGLEN);
  memset(button->name, 0, sizeof(button->name));
  rsm_strncpy_nul2sp((char *)button->name, &message[PKT_IAQT_BUTDATA], IAQT_MSGLEN, length-PKT_IAQT_BUTDATA-3);

  LOG(IAQT_LOG,LOG_DEBUG, "Added Button %d %s - LoadingPage = %s\n",index,button->name,iaqt_page_name(_currentPageLoading));

  // This get's called or every device state change in PDA mode, since we page over all the devices.
  // So capture and update the device state
  if (isPDA_PANEL) {
    updateAQButtonFromPageButton(aqdata,button,0,aqdata->total_buttons);
  } else {
    if ( PANEL_SIZE() >= 16) {
      updateAQButtonFromPageButton(aqdata,button,aqdata->rs16_vbutton_start,aqdata->rs16_vbutton_end + 1);
    }
    if ( isVirtualButtonEnabled() ) {
      updateAQButtonFromPageButton(aqdata,button,aqdata->virtual_button_start,aqdata->total_buttons);
    }
  }
}


// Log if we saw a pump in a device page cycle.
void iaqt_pump_update(struct aqualinkdata *aqdata, int updated) {
  const int bitmask[MAX_PUMPS] = {1,2,4,8};
  static unsigned char updates = '\0';
  //int i;

  if (updated == -1) {
    for(int i=0; i < MAX_PUMPS; i++) {
      if ((updates & bitmask[i]) != bitmask[i]) {
        SET_IF_CHANGED(aqdata->pumps[i].rpm, PUMP_OFF_RPM, aqdata->is_dirty);
        SET_IF_CHANGED(aqdata->pumps[i].gpm, PUMP_OFF_GPM, aqdata->is_dirty);
        SET_IF_CHANGED(aqdata->pumps[i].watts, PUMP_OFF_WAT, aqdata->is_dirty);
        SET_IF_CHANGED(aqdata->pumps[i].pStatus, PS_OFF, aqdata->is_dirty);
        // If Virtual pump, turn it off as well.
        if (i < aqdata->num_pumps && isVBUTTON(aqdata->pumps[i].button->special_mask) && aqdata->pumps[i].button->led->state == ON ) {
          LOG(IAQT_LOG,LOG_DEBUG, "Turning off virtual button pump %d - \n",aqdata->pumps[i].pumpIndex,aqdata->pumps[i].button->label);
          SET_IF_CHANGED(aqdata->pumps[i].button->led->state, OFF, aqdata->is_dirty);
        }
        LOG(IAQT_LOG,LOG_DEBUG, "Clearing pump %d\n",i);
      }
    }
    updates = '\0';
  } else if (updated >=0 && updated < MAX_PUMPS) {
    updates |= bitmask[updated];
    LOG(IAQT_LOG,LOG_DEBUG, "Got pump update message for pump %d\n",updated);
        if (updated < aqdata->num_pumps && isVBUTTON(aqdata->pumps[updated].button->special_mask) && aqdata->pumps[updated].button->led->state == OFF) {
          LOG(IAQT_LOG,LOG_DEBUG, "Turning on virtual button pump %d - %s\n",aqdata->pumps[updated].pumpIndex,aqdata->pumps[updated].button->label);
          SET_IF_CHANGED(aqdata->pumps[updated].button->led->state, ON, aqdata->is_dirty);
        }
  }
}


/* Some newer revisions support pump by name and not number.
   Seen this is rev Yg   Looks like this ID came in T.2g (not T.2 though)

Info:    iAQ Touch: Page: Status (diff ID) | 0x2a\
Info:    iAQ Touch: Status page 00| Filter Pump\
Info:    iAQ Touch: Status page 01| Spa Light\
Info:    iAQ Touch: Status page 02| Pool Light\
Info:    iAQ Touch: Status page 03| Pump 2248\     <--- This is the pump name.
Info:    iAQ Touch: Status page 04|     RPM: 1350\
Info:    iAQ Touch: Status page 05|   Watts: 176\
Info:    iAQ Touch: Status page 06| #1 AquaPure\

*/

pump_detail *matchPump(const logmask_t from, struct aqualinkdata *aqdata, char *name, int *pump_index)
{
  int i;
  pump_detail *pump = NULL;
  int pi = 0;

  LOG(IAQT_LOG,LOG_DEBUG, "Finding pump for message '%s'\n",name);

  if (rsm_strcmp(name, "Intelliflo VS") == 0 ||
      rsm_strcmp(name, "Intelliflo VF") == 0 ||
      rsm_strcmp(name, "Jandy ePUMP") == 0 ||
      rsm_strcmp(name, "ePump AC") == 0)
  {
    pi = rsm_atoi(&name[14]);
    if (pi <= 0)
      pi = rsm_atoi(&name[10]); // ePump AC seems to display index in different position
  }

  // Now loop over all the VSP pumps and check the name.
  for (i = 0; i < aqdata->num_pumps; i++)
  {
    if ((pi > 0 && aqdata->pumps[i].pumpIndex == pi) ||
        (rsm_strcmp(name, aqdata->pumps[i].pumpName) == 0))
    {
      *pump_index = i;
      pump = &aqdata->pumps[i];
    }
  }

  // Log a warning
  if (pi > 0)
  {
    if (pump == NULL)
    {
      LOG(from, LOG_INFO, "Got pump message '%s' but can't find pump # %d, please update aqualinkd.conf\n", name, pi);
    }
    else if (pump->pumpType == PT_UNKNOWN)
    {
      if (rsm_strcmp(name, "Intelliflo VS") == 0)
        SET_IF_CHANGED(pump->pumpType, VSPUMP, aqdata->is_dirty);
      else if (rsm_strcmp(name, "Intelliflo VF") == 0)
        SET_IF_CHANGED(pump->pumpType, VFPUMP, aqdata->is_dirty);
      else if (rsm_strcmp(name, "Jandy ePUMP") == 0 ||
               rsm_strcmp(name, "ePump AC") == 0)
        SET_IF_CHANGED(pump->pumpType, EPUMP, aqdata->is_dirty);
    }
  }

  if (pump == NULL)
    LOG(from, LOG_DEBUG, "Did not find pump config for '%s'\n", name);
  else
    LOG(from, LOG_DEBUG, "Found pump '%s', Name='%s', 'Index='%d'\n", name, pump->pumpName, pump->pumpIndex);

  return pump;
}

void passDeviceStatusPage(struct aqualinkdata *aqdata)
{
  int i;
  int pi = -2;
  pump_detail *pump = NULL;
  //bool found_swg = false;
  //int pump_index = 0;

  for (i=0; i <IAQ_STATUS_PAGE_LINES; i++ ) {
    if (rsm_strcmp(_deviceStatus[i],"RPM:") == 0) {
      pump = matchPump(IAQT_LOG, aqdata, _deviceStatus[i-1], &pi);
      if (pump != NULL) {
        iaqt_pump_update(aqdata, pi); // Log that we saw a pump
        SET_IF_CHANGED(pump->rpm, rsm_atoi(&_deviceStatus[i][9]), aqdata->is_dirty);
        SET_IF_CHANGED(pump->pStatus, PS_OK, aqdata->is_dirty);
      } else
        LOG(IAQT_LOG,LOG_INFO, "Got pump message '%s' but can't find pump\n",_deviceStatus[i]);
      continue;
    } else if (rsm_strcmp(_deviceStatus[i],"GPM:") == 0) {
      if (pump != NULL) {
        SET_IF_CHANGED(pump->gpm, rsm_atoi(&_deviceStatus[i][9]), aqdata->is_dirty);
        SET_IF_CHANGED(pump->pStatus, PS_OK, aqdata->is_dirty);
      } else
        LOG(IAQT_LOG,LOG_INFO, "Got pump message '%s' but can't find pump\n",_deviceStatus[i]);
      continue;
    } else if (rsm_strcmp(_deviceStatus[i],"Watts:") == 0) {
      if (pump != NULL) {
        SET_IF_CHANGED(pump->watts, rsm_atoi(&_deviceStatus[i][9]), aqdata->is_dirty);
        //pump->ppStatus = DON"T SET, WE GET WATTS IN PRIMING
      } else
        LOG(IAQT_LOG,LOG_INFO, "Got pump message '%s' but can't find pump\n",_deviceStatus[i]);
      continue;
    } else if (rsm_strcmp(_deviceStatus[i],"*** Priming ***") == 0) {
      pump = matchPump(IAQT_LOG, aqdata, _deviceStatus[i-1], &pi);
      if (pump != NULL) {
        iaqt_pump_update(aqdata, pi); // Log that we saw a pump
        SET_IF_CHANGED(pump->pStatus, PS_PRIMING, aqdata->is_dirty);
      } else
        LOG(IAQT_LOG,LOG_INFO, "Got pump message '%s' but can't find pump\n",_deviceStatus[i]);
      continue;
    } else if (rsm_strcmp(_deviceStatus[i],"(Offline)") == 0) {
      pump = matchPump(IAQT_LOG, aqdata, _deviceStatus[i-1], &pi);
      if (pump != NULL) {
        iaqt_pump_update(aqdata, pi); // Log that we saw a pump
        SET_IF_CHANGED(pump->pStatus, PS_OFFLINE, aqdata->is_dirty);
      } else
        LOG(IAQT_LOG,LOG_INFO, "Got pump message '%s' but can't find pump\n",_deviceStatus[i]);
      continue;
    } else if (rsm_strcmp(_deviceStatus[i],"(Priming Error)") == 0) {
      pump = matchPump(IAQT_LOG, aqdata, _deviceStatus[i-1], &pi);
      if (pump != NULL) {
        iaqt_pump_update(aqdata, pi); // Log that we saw a pump
        SET_IF_CHANGED(pump->pStatus, PS_ERROR, aqdata->is_dirty);
      } else
        LOG(IAQT_LOG,LOG_INFO, "Got pump message '%s' but can't find pump\n",_deviceStatus[i]);
      continue;
      // Need to catch messages like 
      // *** Priming ***
      // (Priming Error)
      // (Offline)
    } else {
      pump = NULL;
    }
    if (rsm_strcmp(_deviceStatus[i],"Chemlink") == 0) {
      /*   Info:  =    Chemlink 1   
           Info:  =  ORP 750/PH 7.0  */
      // Use logic under TrueSense below to get ORP & PH
      i++;
    }

    /* TruSense is different format to chemlink above
                   | ORP:810 pH:7.3
        Also make compatable with msg with intent of removing above Chemlink
                     ORP 750/PH 7.0
    */
    if (rsm_strcmp(_deviceStatus[i],"ORP") == 0) {
      char *oi = rsm_strncasestr(_deviceStatus[i], "ORP", AQ_MSGLEN);
      char *pi = rsm_strncasestr(_deviceStatus[i], "PH", AQ_MSGLEN);
      if (oi != NULL && pi != NULL) {
        int orp = rsm_atoi(oi+4);
        float ph = rsm_atof(pi+3);
        if (orp > 0 && ph > 0) {
          if (aqdata->ph != ph || aqdata->orp != orp) {
            SET_IF_CHANGED(aqdata->ph, ph, aqdata->is_dirty);
            SET_IF_CHANGED(aqdata->orp, orp, aqdata->is_dirty);
          }
          LOG(IAQT_LOG,LOG_INFO, "Set Cem ORP = %d PH = %f from message '%s'\n",orp,ph,_deviceStatus[i]);
        } else {
          // Didn't understand message, not
          LOG(IAQT_LOG,LOG_DEBUG, "Could not extract ORP & PH from message '%s'\n",_deviceStatus[i]);
        }
      }
    }
    
    if (ENABLE_CHILLER && (rsm_strcmp(_deviceStatus[i],"Chiller") == 0 || rsm_strcmp(_deviceStatus[i],"Heat Pump") == 0 )) {
      processHeatPumpDisplayMessage(_deviceStatus[i], aqdata);
    }   
    
//#ifdef READ_SWG_FROM_EXTENDED_ID
    else if (isPDA_PANEL) {
     if (rsm_strcmp(_deviceStatus[i],"AQUAPURE") == 0) {
      //aqdata->swg_percent = rsm_atoi(&_deviceStatus[i][9]);
      if (changeSWGpercent(aqdata, rsm_atoi(&_deviceStatus[i][9])))
        LOG(IAQT_LOG,LOG_DEBUG, "Set swg %% to %d from message'%s'\n",aqdata->swg_percent,_deviceStatus[i]);
     } else if (rsm_strcmp(_deviceStatus[i],"salt") == 0) {
      SET_IF_CHANGED(aqdata->swg_ppm, rsm_atoi(&_deviceStatus[i][5]), aqdata->is_dirty);
      LOG(IAQT_LOG,LOG_DEBUG, "Set swg PPM to %d from message'%s'\n",aqdata->swg_ppm,_deviceStatus[i]);
     } else if (rsm_strcmp(_deviceStatus[i],"Boost Pool") == 0) {
      SET_IF_CHANGED(aqdata->boost, true, aqdata->is_dirty);
      // Let RS pickup time remaing message.
     }
    }
//#endif

  } // for
}

void debugPrintButtons(struct iaqt_page_button buttons[])
{
  int i;
  for (i=0; i < IAQ_PAGE_BUTTONS; i++) {
    if (buttons[i].state != 0 || buttons[i].type != 0 || buttons[i].unknownByte != 0 || buttons[i].keycode != 0)
      LOG(IAQT_LOG,LOG_DEBUG, "Button %.2d| %21.21s | type=0x%02hhx | state=0x%02hhx | unknown=0x%02hhx | keycode=0x%02hhx\n",i,buttons[i].name,buttons[i].type,buttons[i].state,buttons[i].unknownByte,buttons[i].keycode);
  }
}

//#define member_size(type, member) (sizeof( ((type*)0)->member ))

void processPage(struct aqualinkdata *aqdata)
{
  //static int _home_cnt = 0;
  int i;
  int dp = 0;

  LOG(IAQT_LOG,LOG_INFO, "Page: %s | 0x%02hhx\n",iaqt_page_name(_currentPage),_currentPage);

  switch(_currentPage) {
    case IAQ_PAGE_STATUS:
    case IAQ_PAGE_STATUS2:
      //LOG(IAQT_LOG,LOG_INFO, "Status Page:-\n");
      for (i=0; i <IAQ_STATUS_PAGE_LINES; i++ )
        if (strlen(_deviceStatus[i]) > 1)
          LOG(IAQT_LOG,LOG_INFO, "Status page %.2d| %s\n",i,_deviceStatus[i]);
      
      debugPrintButtons(_pageButtons);
      passDeviceStatusPage(aqdata);
      // If button 1 is type 0x02 then there is a next page.  Since status page isn't used for programming, hit the next page button.
      if (_pageButtons[1].type == 0x02) {
        iaqt_queue_cmd(KEY_IAQTCH_KEY02);
      } else {
        iaqt_pump_update(aqdata, -1); // Reset pumps.
        //iaqt_device_update(aqdata,DEVICE_RESET); // Reset any devices we monitor.
        if ( (isPDA_PANEL || PANEL_SIZE() >= 16) && !in_iaqt_programming_mode(aqdata) ) {
          iaqt_queue_cmd(KEY_IAQTCH_HOME);
        } 
      }
    break;
    case IAQ_PAGE_DEVICES:
    case IAQ_PAGE_DEVICES2:
    case IAQ_PAGE_DEVICES3:
    case IAQ_PAGE_DEVICES_REV_Yg:
      if (_currentPage == IAQ_PAGE_DEVICES)
        dp = 0;
      else if (_currentPage == IAQ_PAGE_DEVICES_REV_Yg)
        dp = 0;
      else if (_currentPage == IAQ_PAGE_DEVICES2)
        dp = 1;
      else if (_currentPage == IAQ_PAGE_DEVICES3)
        dp = 2;
      //LOG(IAQT_LOG,LOG_INFO, "Devices Page #1:-\n");
      debugPrintButtons(_devicePageButtons[dp]);
#ifndef NEW_POLL_CYCLE
      // If Button 15 has type 0x02 then we have previous, if 0x00 nothing (previous send code KEY_IAQTCH_PREV_PAGE)
      // If Button 16 has type 0x03 then we have next, if 0x00 nothing (next send code KEY_IAQTCH_NEXT_PAGE)
      if ( (isPDA_PANEL || isVirtualButtonEnabled() || PANEL_SIZE() >= 16) && !in_iaqt_programming_mode(aqdata) ) {
        if (_devicePageButtons[dp][16].type == 0x03) {
          iaqt_queue_cmd(KEY_IAQTCH_NEXT_PAGE);
        } else {
          iaqt_queue_cmd(KEY_IAQTCH_STATUS);
        }
      }
#endif
    break;
    case IAQ_PAGE_COLOR_LIGHT:
      //LOG(IAQT_LOG,LOG_INFO, "Color Light Page :-\n");
      debugPrintButtons(_pageButtons);
    break;
    case IAQ_PAGE_HOME:
      //LOG(IAQT_LOG,LOG_INFO, "Home Page :-\n");
      for (i=0; i <IAQ_STATUS_PAGE_LINES; i++ )
        if (_deviceStatus[i][0] != 0)
          LOG(IAQT_LOG,LOG_INFO, "Status page %.2d| %s\n",i,_deviceStatus[i]);

      for (i=0; i <IAQ_STATUS_PAGE_LINES; i++ )
        if (_homeStatus[i][0] != 0)
          LOG(IAQT_LOG,LOG_INFO, "Home Status page %.2d| %s\n",i,_homeStatus[i]);

//Info:    iAQ Touch: Home Status page 00| 65  
//Info:    iAQ Touch: Home Status page 01| 72  
//Info:    iAQ Touch: Home Status page 04| Spa Temp
//Info:    iAQ Touch: Home Status page 05| Air Temp
// IF WE HAVE A PANEL WITH EXTRA PUMP & TEMP SENSOR, THEN WE GET DIFFERENT INFO.
// We Need to code for both cases.
//Info:   iAQ Touch: Home Status page 00| 96          <- pool
//Info:   iAQ Touch: Home Status page 01| 96          <- air
//Info:   iAQ Touch: Home Status page 02| Spa Temp
//Info:   iAQ Touch: Home Status page 03| 95          <- spa temp
//Info:   iAQ Touch: Home Status page 04| Pool Temp
//Info:   iAQ Touch: Home Status page 05| Air Temp

      if (isPDA_PANEL) { // Set temp if PDA panel
       if (rsm_strcmp(_homeStatus[5],"Air Temp") == 0) {
        SET_IF_CHANGED(aqdata->air_temp, atoi(_homeStatus[1]), aqdata->is_dirty);
        LOG(IAQT_LOG,LOG_DEBUG, "Air Temp set to %d\n",aqdata->air_temp);
       }
       if (rsm_strcmp(_homeStatus[4],"Pool Temp") == 0) {
        SET_IF_CHANGED(aqdata->pool_temp, atoi(_homeStatus[0]), aqdata->is_dirty);
        LOG(IAQT_LOG,LOG_DEBUG, "Pool Temp set to %d\n",aqdata->air_temp);
       } else if (rsm_strcmp(_homeStatus[4],"Spa Temp") == 0) {
        SET_IF_CHANGED(aqdata->spa_temp, atoi(_homeStatus[0]), aqdata->is_dirty);
        LOG(IAQT_LOG,LOG_DEBUG, "Spa Temp set to %d\n",aqdata->spa_temp);
       }
      }

      //passHomePage(aqdata);
      debugPrintButtons(_homeButtons);    
    break;
    case IAQ_PAGE_SYSTEM_SETUP:
      //LOG(IAQT_LOG,LOG_INFO, "System Setup :-\n");
      debugPrintButtons(_deviceSystemSetupButtons[0]);
    break;
    case IAQ_PAGE_SYSTEM_SETUP2:
      //LOG(IAQT_LOG,LOG_INFO, "System Setup :-\n");
      debugPrintButtons(_deviceSystemSetupButtons[1]);
    break;
    case IAQ_PAGE_SYSTEM_SETUP3:
      //LOG(IAQT_LOG,LOG_INFO, "System Setup :-\n");
      debugPrintButtons(_deviceSystemSetupButtons[2]);
    break;
    case IAQ_PAGE_SET_VSP:
      debugPrintButtons(_pageButtons);
    break;
    case IAQ_PAGE_HELP:
    /*
    Info:    iAQ Touch: Table Messages 01| Interface: AquaLink Touch
    Info:    iAQ Touch: Table Messages 02| Model: RS-8 Combo
    Info:    iAQ Touch: Table Messages 03| AquaLink: REV T.0.1
    Info:    iAQ Touch: Table Messages 04| CPU p/n: B0029221
    Info:    iAQ Touch: Table Messages 05| TL Rev:   
    */
      
      setPanelInformationFromPanelMsg(aqdata, (char *)_tableInformation[2], PANEL_STRING, IAQTOUCH);
      setPanelInformationFromPanelMsg(aqdata, (char *)_tableInformation[3], PANEL_REV, IAQTOUCH);
      setPanelInformationFromPanelMsg(aqdata, (char *)_tableInformation[4], PANEL_CPU, IAQTOUCH);
/*
      if (isPDA_PANEL && ((char *)_tableInformation[03]) > 0) {
        if ( rsm_get_revision(aqdata->revision,(char *)_tableInformation[3], sizeof(aqdata->revision) ) == TRUE) {
          int len = rsm_get_boardcpu(aqdata->version, sizeof(aqdata->version), (char *)_tableInformation[4], IAQT_TABLE_MSGLEN );
          sprintf(aqdata->version+len, " REV %s",aqdata->revision);
          LOG(IAQT_LOG,LOG_NOTICE, "Control Panel revision %s\n",  aqdata->revision);
          LOG(IAQT_LOG,LOG_NOTICE, "Control Panel version %s\n",  aqdata->version);
          SET_DIRTY(aqdata->is_dirty);
        }
      }
*/

    break;

    default:
      //LOG(IAQT_LOG,LOG_INFO, "** UNKNOWN PAGE 0x%02hhx **\n",_currentPage);
      for (i=0; i <IAQ_STATUS_PAGE_LINES; i++ )
        if (_deviceStatus[i][0] != 0)
          LOG(IAQT_LOG,LOG_INFO, "Page %.2d| %s\n",i,_deviceStatus[i]);

      debugPrintButtons(_pageButtons);
    break;
  }

  for (i=0; i < IAQ_MSG_TABLE_LINES; i++) {
    if (strlen((char *)_tableInformation[i]) > 0)
      LOG(IAQT_LOG,LOG_INFO, "Table Messages %.2d| %s\n",i,_tableInformation[i]);
  }
}


//#define REQUEST_DEVICES_POLL_COUNT 30 // if _aqconfig_.enable_iaqualink=true then REQUEST_STATUS_POLL_COUNT will be used.

static int _pollCnt;

// running through status while programming a lighgt seems to confuse the panel, so let
// other people reset our poll count.
#ifdef PLIGHT_IAQTOUCH_FIX
void reset_iaqTouchPollCounter(int number)
{
  _pollCnt = number;
}
void delay_iaqTouch4lightProgramming()
{
  reset_iaqTouchPollCounter(-100); // Total quess at the moment.
}
int get_iaqTouchPollCounter()
{
  return _pollCnt;
}
#endif

bool process_iaqtouch_packet(unsigned char *packet, int length, struct aqualinkdata *aqdata)
{
  static bool gotInit = false; 
  //static int _pollCnt = 0;
  //static int probesSinceLastPageCMD=0;
  static bool gotStatus = true;
  //static char message[AQ_MSGLONGLEN + 1];
  bool fake_pageend = false;
  //char buff[1024];
  // NSF Take this out
  
  /*
  if ( packet[3] != CMD_IAQ_POLL && getLogLevel(IAQT_LOG) >= LOG_DEBUG ) {
  //if ( getLogLevel(IAQT_LOG) >= LOG_DEBUG ) {
    char buff[1024];
    beautifyPacket(buff, 1024, packet, length, true);
    LOG(IAQT_LOG,LOG_DEBUG, "Received message : %s", buff);
  }*/

  // DEBUG for iAqualink
  /*
  if (_aqconfig_.enable_iaqualink) {
    if (packet[PKT_CMD] == CMD_IAQ_AUX_STATUS) {
      // Look at notes in iaqualink.c for how this packet is made up
      int start=packet[4];
      start = start + 5;
      for (int i = start; i < length-3; i=i) {
        int status=i;
        int labelstart=status+5;
        int labellen=packet[status+4];
        if (labelstart+labellen < length) {
          LOG(IAQT_LOG,LOG_NOTICE, "Label %.*s = %s, bit1=0x%02hhx bit2=0x%02hhx bit3=0x%02hhx bit4=0x%02hhx\n", labellen, &packet[labelstart], (packet[status]==0x00?"Off":"On "), packet[status],packet[status+1],packet[status+2],packet[status+3]);
        }
        i = labelstart + labellen;
      }
    
      LOG(IAQT_LOG,LOG_NOTICE, "Pump %s, Spa %s, SWG %d, PumpRPM %d, PoolSP=%d, SpaSP=%d\n",
                              aqdata->aqbuttons[0].led->state==OFF?"Off":"On ",
                              aqdata->aqbuttons[1].led->state==OFF?"Off":"On ",
                              aqdata->swg_percent,
                              aqdata->pumps[0].rpm,
                              aqdata->pool_htr_set_point,
                              aqdata->spa_htr_set_point);
    } else if (packet[PKT_CMD] == 0x70){
      debuglogPacket(IAQT_LOG, packet, length, true, true);
    } else if (packet[PKT_CMD] == 0x71){ 
      debuglogPacket(IAQT_LOG, packet, length, true, true);
    }
  }*/
  if (packet[PKT_CMD] == CMD_IAQ_MAIN_STATUS ||
      packet[PKT_CMD] == CMD_IAQ_1TOUCH_STATUS ||
      packet[PKT_CMD] == CMD_IAQ_AUX_STATUS) {
    process_iaqualink_packet(packet, length, aqdata);
  } 
  else if (packet[PKT_CMD] == CMD_IAQ_PAGE_START) 
  {
    // Reset and messages on new page
    //aqdata->last_display_message[0] = ' ';
    //aqdata->last_display_message[1] = '\0';
    SET_IF_CHANGED_STRCPY( aqdata->last_display_message, "", aqdata->is_dirty);
    SET_IF_CHANGED(aqdata->is_display_message_programming, false, aqdata->is_dirty);
    LOG(IAQT_LOG,LOG_DEBUG, "Turning IAQ SEND off\n");
    set_iaq_cansend(false);
    _currentPageLoading = packet[PKT_IAQT_PAGTYPE];
    _currentPage = NUL;
    memset(_pageButtons, 0, IAQ_PAGE_BUTTONS * sizeof(struct iaqt_page_button));
    memset(_deviceStatus, 0, sizeof(char) * IAQ_STATUS_PAGE_LINES * AQ_MSGLEN+1 );
    memset(_tableInformation, 0, sizeof(char) * IAQ_MSG_TABLE_LINES * AQ_MSGLEN+1 );
    //memset(_devicePageButtons, 0, IAQ_PAGE_BUTTONS * sizeof(struct iaqt_page_button));
    // Fix bug with control panel where after a few hours status page disapears and you need to hit menu.
    if (gotStatus == false)
      gotStatus = true;
    //[IAQ_STATUS_PAGE_LINES][AQ_MSGLEN+1];
  } 
  else if (packet[PKT_CMD] == CMD_IAQ_PAGE_END) 
  {
    set_iaq_cansend(true);
    LOG(IAQT_LOG,LOG_DEBUG, "Turning IAQ SEND on\n");
    if (_currentPageLoading != NUL) {
      _currentPage = _currentPageLoading;
      //_currentPageLoading = NUL;
    } else {
      LOG(IAQT_LOG,LOG_DEBUG, "Page end message without proceding page start, ignoring!\n");
    }

    if (isPDA_PANEL) {
      // Time is in the page end command
      //    1/18/2011 13:42
      //Hex   |0x10|0x02|0x33|0x28|0x01|0x12|0x0b|0x0d|0x2a|0xc2|0x10|0x03|
      //Dec   |  16|   2|  51|  40|   1|  18|  11|  13|  42| 194|  16|   3
      //Ascii |    |    |   3|   (|    |    |    |    |   *|    |    |    
      snprintf(aqdata->date, sizeof(aqdata->date), "%02d/%02d/%02d", packet[4],packet[5],packet[6]);
      if (packet[7] <= 12)
        snprintf(aqdata->time, sizeof(aqdata->date), "%d:%02d am", packet[7],packet[8]);
      else
        snprintf(aqdata->time, sizeof(aqdata->date), "%d:%02d pm", (packet[7] - 12),packet[8]);
    }

    processPage(aqdata);
    
  } else if (packet[PKT_CMD] == CMD_IAQ_TABLE_MSG) {
    processTableMessage(packet, length);
  } else if (packet[PKT_CMD] == CMD_IAQ_PAGE_MSG) {
    processPageMessage(packet, length);
  } else if (packet[PKT_CMD] == CMD_IAQ_PAGE_BUTTON) {
    processPageButton(packet, length, aqdata);
    // Second page on status doesn't send start & end, but button is message, so use that to kick off next page. 
    if (_currentPage == IAQ_PAGE_STATUS) {
      /* Notice: Added Button 1 
       * Notice: To 0x33 of type iAq pBut | HEX: 0x10|0x02|0x33|0x24|0x01|0x00|0x00|0x00|0x00|0x00|0x6a|0x10|0x03|
       *                   Button | 1 | 0x00 |  |-|  |-|  -off-
       * 
       *   SHOULD PROBABLY USE ABOVE TO CHECK.
       */
      //if (packet[PKT_IAQT_BUTTYPE] == 0x02 )
       processPage(aqdata);
    }
    
    // if we get a button with 0x00 state on Light Page, that's the end of page.
    if (_currentPageLoading == IAQ_PAGE_COLOR_LIGHT) {
      if (packet[7] == 0x00 && packet[4] == 0x0e) { // packet[4] is button number 0x0e is 14 (last button)
        //printf("** MANUAL PAGE END\n");
        LOG(IAQT_LOG,LOG_DEBUG, "MANUAL PAGE END\n");
        _currentPage = _currentPageLoading;
        //_currentPageLoading = NUL;
        processPage(aqdata);
        set_iaq_cansend(true);
        // For programming mode 
        fake_pageend = true;
        // Also END page here, as you can send commands.
        // NEED to rethink this approach.  ie, selecting light needs to hold open while showing page, no page end, then select light color, then message "please wait", the finally done 
      }
    }
  //} else if (isPDA_PANEL && packet[PKT_CMD] == CMD_IAQ_MSG_LONG) { // might want to add isPDA or JandyInfinateLight
  } else if (packet[PKT_CMD] == CMD_IAQ_MSG_LONG) {
    char *sp;
    // Set disply message if PDA panel
    memset(_popupMsg, 0, AQ_MSGLONGLEN + 1);
    rsm_strncpy(_popupMsg, packet + 6, AQ_MSGLONGLEN, length-9);
    LOG(IAQT_LOG,LOG_INFO, "Popup message '%s'\n",_popupMsg);
    
    // Change this message, since you can't press OK.  'Light will turn off in 5 seconds. To change colors press Ok now.'
     if ((sp = rsm_strncasestr(_popupMsg, "To change colors press Ok now", strlen(_popupMsg))) != NULL)
     {
       *sp = '\0';
     }

    SET_IF_CHANGED_STRCPY(aqdata->last_display_message, _popupMsg, aqdata->is_dirty); // Also display the message on web UI
   
    if (in_programming_mode(aqdata)) {
      SET_IF_CHANGED(aqdata->is_display_message_programming, true, aqdata->is_dirty);
    } else {
      SET_IF_CHANGED(aqdata->is_display_message_programming, false, aqdata->is_dirty);
    }
    /*
    for(int i=0; i<length; i++) {
      printf("0x%02hhx|",packet[i]);
    }
    printf("\n");
    */
  }
  
  if (packet[3] == 0x29) {
    //printf("*****  iAqualink Touch STARTUP Message ******* \n");
    if (gotInit == false) {
      LOG(IAQT_LOG,LOG_DEBUG, "STARTUP Message\n");
      
      if (_aqconfig_.enable_iaqualink) {
        
        // Below will not send 0x29 since queueGetProgramData takes presidance, 
        //iaqt_queue_cmd(0x29);
        if (isPDA_PANEL) {
          _aqconfig_.extended_device_id2 = _aqconfig_.device_id + 112;
        } else {
          _aqconfig_.extended_device_id2 = _aqconfig_.extended_device_id + 112; // 0x70 in dec
        }
        LOG(IAQT_LOG,LOG_NOTICE, "Enabling iAqualink Protocol on 0x%02hhx\n",_aqconfig_.extended_device_id2);
      }
      // Don't like this here.  Come back and rethink getting panel string.
      //iaqt_queue_cmd(KEY_IAQTCH_HELP);
      //iaqt_queue_cmd(KEY_IAQTCH_HOME);
      
      queueGetProgramData(IAQTOUCH, aqdata);
      gotInit = true;
    }

    //aq_programmer(AQ_SET_IAQTOUCH_SET_TIME, NULL, aqdata);
  }
  /*
  NEED TO ALTERNATE SEND KEY_IAQTCH_HOMEP_KEY08 KEY and KEY_IAQTCH_STATUS BELOW FOR PDA
  */

  // Standard ack/poll
  if (packet[3] == CMD_IAQ_POLL) {
    //probesSinceLastPageCMD++;

#ifdef NEW_POLL_CYCLE
/*
// loop device/<pause>device1/device2/device3/status/home/<repeat>
----------
if not programming && poll packet {
  if home page end loaded {
    reset counter
    goto device page.
  }

  if (counter 5 ) {
    if device goto device 1
    else if device1 goto device2
    else if device2 goto device3
    else if device3 goto status
    else if status goto Home
  }
  increase counter

  The above sits on device page.
  Should we sit on status page to get better updates of VSP/SWG?  would make sence if iaqualink is enabeled
  since that gets all device updates quicker and cleaner.
  if we a PDA only, then sit on devices page.
  if iaqualink is not enabeled, still sit on status page.
  So why did I update this to sit on devices page???????????
  RS16 should also probably sit on devices page.
}*/
    if (in_programming_mode(aqdata) == false) {
      //LOG(IAQT_LOG,LOG_DEBUG, "Poll counter = %d\n",_pollCnt);

      if (_currentPage == IAQ_PAGE_HOME) {
        iaqt_queue_cmd(KEY_IAQTCH_HOMEP_KEY08); // This is "other devices on/off" page
        _pollCnt = 0;
      }

      //if ( (isPDA_PANEL || isVirtualButtonEnabled() || PANEL_SIZE() >= 16) && !in_iaqt_programming_mode(aqdata) ) {
      // Just sent Status Page request if none of the above are active
      //}
      // After we send devices page in above if statment, kick us through a loop of
      // devices devices1 devices2 devices2 status.
      // We probably only need to go over this loop if iaqualink2 is NOT enabled.
      // But may be better to simply increase FULL_STATUS_POLL_COUNT when it's not enabled.
      uint8_t nextPageRequestKey = KEY_IAQTCH_HOME;

      if (_pollCnt > FULL_STATUS_POLL_COUNT + 5) {
        LOG(IAQT_LOG,LOG_ERR,"Poll count=%d, too high, looks like page is stuck\n",_pollCnt);
        _pollCnt=0;
      } else {
        //LOG(IAQT_LOG,LOG_DEBUG,"Poll count=%d, Curent Page=0x%02hhx\n",_pollCnt, _currentPage);
      }

      if (_pollCnt++ > FULL_STATUS_POLL_COUNT) {
        switch(_currentPage) {
          case IAQ_PAGE_DEVICES:
          case IAQ_PAGE_DEVICES_REV_Yg:
            if (_devicePageButtons[0][16].type == 0x03) 
              nextPageRequestKey = KEY_IAQTCH_NEXT_PAGE;
            else
              nextPageRequestKey = KEY_IAQTCH_STATUS;
          break;
          case IAQ_PAGE_DEVICES2:
            if (_devicePageButtons[1][16].type == 0x03) 
              nextPageRequestKey = KEY_IAQTCH_NEXT_PAGE;
            else
              nextPageRequestKey = KEY_IAQTCH_STATUS;
          break;
          case IAQ_PAGE_DEVICES3:
            if (_devicePageButtons[1][16].type == 0x03) 
              nextPageRequestKey = KEY_IAQTCH_NEXT_PAGE;
            else
              nextPageRequestKey = KEY_IAQTCH_STATUS;
          break;
        }

        iaqt_queue_cmd(nextPageRequestKey);
      
      } else if ( (_pollCnt > 0) && (_pollCnt % DEVICE_STATUS_POLL_COUNT == 0) && 
                  (_currentPage == IAQ_PAGE_DEVICES || _currentPage == IAQ_PAGE_DEVICES_REV_Yg) ) {
        iaqt_queue_cmd(KEY_IAQTCH_STATUS); // This will force us to go to status, then it'll jump back to devices, then force status again
      }
    } else if (in_programming_mode(aqdata) == true) {
      // Set count to something close to max, so we will pull latest info once programming has finished.
      // This is good for VSP GPM programming as it takes number of seconds to register once finished programming.
      // -5 seems to be too quick for VSP/GPM so using 10
      // This is probably not needed any more, since we grab status quite often now.
      _pollCnt = FULL_STATUS_POLL_COUNT - 10; 
    }
#else
    //LOG(IAQT_LOG,LOG_DEBUG, "poll count %d\n",_pollCnt);
    // Load status page every 50 messages
    if (_pollCnt++ > FULL_STATUS_POLL_COUNT && in_programming_mode(aqdata) == false ) {
      if (isPDA_PANEL || PANEL_SIZE() >= 16) {
        iaqt_queue_cmd(KEY_IAQTCH_HOMEP_KEY08);
      } else {
        iaqt_queue_cmd(KEY_IAQTCH_STATUS);
      }
      gotStatus = false; // Reset if we got status page, for fix panel bug.
      //aq_programmer(AQ_GET_IAQTOUCH_VSP_ASSIGNMENT, NULL, aqdata);
      _pollCnt = 0;
    } else if (gotStatus == false && _pollCnt > 3) {
      // Fix bug with control panel where after a few hours status page disapears and you need to hit menu.
      LOG(IAQT_LOG,LOG_INFO, "Overcomming Jandy control panel bug, (missing status, goto menu)\n",_pollCnt);
      iaqt_queue_cmd(KEY_IAQTCH_HOME);
      _pollCnt = FULL_STATUS_POLL_COUNT - 5;
     /* 
      if (isPDA_PANEL) {
        iaqt_queue_cmd(KEY_IAQTCH_HOMEP_KEY08);
      } else {
        iaqt_queue_cmd(KEY_IAQTCH_STATUS);
      }
     */


    } else if (in_programming_mode(aqdata) == true) {
      // Set count to something close to max, so we will pull latest info once programming has finished.
      // This is good for VSP GPM programming as it takes number of seconds to register once finished programming.
      // -5 seems to be too quick for VSP/GPM so using 10
      _pollCnt = FULL_STATUS_POLL_COUNT - 10; 
    }

    // On poll no need to kick programming threads
    return false;
#endif
  }

  //debuglogPacket(IAQT_LOG ,packet, length);

  //_lastMsgType = packet[PKT_CMD];
  if (fake_pageend){
    set_iaqtouch_lastmsg(CMD_IAQ_PAGE_END);
  } else {
    set_iaqtouch_lastmsg(packet[PKT_CMD]);
  }
  //debuglogPacket(IAQT_LOG ,packet, length);
  //beautifyPacket(buff, packet, length);
  //LOG(IAQT_LOG,LOG_DEBUG, "%s", buff);

  //temp_debugprintExtraInfo(packet, length);

  kick_aq_program_thread(aqdata, IAQTOUCH);

  return true;
}

//char _namebuf[40];

const char *iaqt_page_name(const unsigned char page)
{
  static char _namebuf[40];

  switch (page){
    case IAQ_PAGE_HOME:
      return "HOME";
    break;
    case IAQ_PAGE_STATUS:
      return "Status";
    break;
    case IAQ_PAGE_STATUS2:
      return "Status (diff ID)";
    break;
    case IAQ_PAGE_DEVICES:
      return "Devices #1";
    break;
    case IAQ_PAGE_DEVICES2:
      return "Devices #2";
    break;
    case IAQ_PAGE_DEVICES3:
      return "Devices #2";
    break;
    case IAQ_PAGE_SET_TEMP:
      return "Set Temp";
    break;
    case IAQ_PAGE_MENU:
      return "MENU";
    break;
    case IAQ_PAGE_SET_VSP:
      return "Set VSP";
    break;
    case IAQ_PAGE_SET_TIME:
      return "Set Time";
    break;
    case IAQ_PAGE_SET_DATE:
      return "Set Date";
    break;
    case IAQ_PAGE_SET_SWG:
      return "Set Aquapure";
    break;
    case IAQ_PAGE_SET_BOOST:
      return "Set Boost";
    break;
    case IAQ_PAGE_SET_QBOOST:
      return "Set Quick Boost";
    break;
    case IAQ_PAGE_ONETOUCH:
      return "OneTouch";
    break;
    case IAQ_PAGE_COLOR_LIGHT:
      return "Color Lights";
    break;
    case IAQ_PAGE_SYSTEM_SETUP:
      return "System Setup";
    break;
    case IAQ_PAGE_SYSTEM_SETUP2:
      return "System Setup #2";
    break;
    case IAQ_PAGE_SYSTEM_SETUP3:
      return "System Setup #3";
    break;
    case IAQ_PAGE_VSP_SETUP:
      return "VSP Setup";
    break;
    case IAQ_PAGE_FREEZE_PROTECT:
      return "Freeze Protect";
    break;
    case IAQ_PAGE_LABEL_AUX:
      return "Label Aux";
    break;
    case IAQ_PAGE_HELP:
      return "Help Page";
    break;
    case IAQ_PAGE_SERVICEMODE:
      return "Service Mode";
    break;

    // New stuff
    case IAQ_PAGE_DEVICES_REV_Yg:
      return "Devices (rev Yg)";
    break;

    default:
      sprintf (_namebuf,"** Unknown 0x%02hhx **",page);
      return _namebuf;
      //return "** Unknown **";
    break;
  }
  return "";
}



void temp_debugprintExtraInfo(unsigned char *pk, int length)
{
  if (pk[PKT_CMD] == CMD_IAQ_PAGE_MSG) {
    int i;
    printf("                         Message | %d | ",(int)pk[4]);
    // Byte #4 is line index on status page at least
    // 1 bytes unknown #4.
    // Message starts at 5
    for (i=5;i<length-3;i++)
      printf("%c",pk[i]);
    printf("\n");
  }
  else if (pk[PKT_CMD] == CMD_IAQ_TABLE_MSG) {
    int i;
    printf("                       Table Msg | %d | ",(int)pk[5]);
    for (i=6;i<length-3;i++)
      printf("%c",pk[i]);
    printf("\n");
  }
  else if (pk[PKT_CMD] == CMD_IAQ_PAGE_BUTTON) {
    int i;
    printf("                          Button | %d | 0x%02hhx | ",(int)pk[4],pk[7]);
    // byte 4 is button index.
    // byte 5 is status 0x00 off (0x01, 0x02, 0x03)
    // byte 6
    // byte 7
    // 6 & 7 0x01|0x00 for normal stuff
    // 6 & 7 0x00|0x0c for lights with 7 incrementing.
    // 7 = Key to send in ACK to select, at least for Light modes.
    // (Might also be in pk5=0x0d) Think if pk6 = 0x01, pk7 not used, if pk=0x00 then pk7 is keycode to send if you select that option.
    // Byte 7 0x03 PAGE DOWN / 0x02 PAGE UP
    // 2 bytes unknown #5 and #6.
    // Message starts at 8
    // if pk[5] == 0x0d start at 8
    for (i=8;i<length-3;i++) {
      // If we get a 0x00 looks like a return.
      if (pk[i] == 0x00)
        printf(" |-| ");
      
      printf("%c",pk[i]);
    }

    if (pk[5] == 0x00)
      printf(" -off- ");
    else if (pk[5] == 0xff)
      printf(" -blank/unknown- ");
    else if (pk[5] == 0x03)
      printf(" -Enabeled- ");
    else if (pk[5] == 0x01)
      printf(" -on- ");
    else if (pk[5] == 0x0d)
      printf(" -Light something 0x0d|0x%02hhx|0x%02hhx %d - ",pk[6],pk[7],(int)pk[7]);
    else
      printf(" -?- 0x%02hhx ",pk[5]);

    printf("\n");
  }

  else if (pk[PKT_CMD] == CMD_IAQ_PAGE_START) {
    if (pk[4] == IAQ_PAGE_STATUS)
      printf("                        New Page | Status\n");
    else if (pk[4] == IAQ_PAGE_HOME)
      printf("                        New Page | Home\n");
    else if (pk[4] == IAQ_PAGE_DEVICES)
      printf("                        New Page | Other Devices\n");
    else if (pk[4] == IAQ_PAGE_DEVICES2)
      printf("                        New Page | Other Devices page 2\n");
    else if (pk[4] == IAQ_PAGE_DEVICES_REV_Yg)
      printf("                        New Page | Devices Panel REV Yg\n");
    else if (pk[4] == IAQ_PAGE_SET_TEMP)
      printf("                        New Page | Set Temp\n");
    else if (pk[4] == IAQ_PAGE_MENU)
      printf("                        New Page | MENU\n");
    else if (pk[4] == IAQ_PAGE_SET_VSP)
      printf("                        New Page | VSP Adjust\n");
    else if (pk[4] == IAQ_PAGE_SET_TIME)
      printf("                        New Page | Set Time\n");
    else if (pk[4] == IAQ_PAGE_SET_DATE)
      printf("                        New Page | Set Date\n");
    else if (pk[4] == IAQ_PAGE_SET_SWG)
      printf("                        New Page | Set Aquapure\n");
    else if (pk[4] == IAQ_PAGE_SET_BOOST)
      printf("                        New Page | Set BOOST\n");
    else if (pk[4] == IAQ_PAGE_SET_QBOOST)
      printf("                        New Page | Set Quick BOOST\n");
    else if (pk[4] == IAQ_PAGE_ONETOUCH)
      printf("                        New Page | One Touch\n");
    else if (pk[4] == IAQ_PAGE_COLOR_LIGHT)
      printf("                        New Page | Color Light\n");
    else
      printf("                        New Page | unknown 0x%02hhx\n",pk[4]);
  }
  else if (pk[PKT_CMD] == CMD_IAQ_PAGE_END) {
    printf("                        New Page | Finished\n");
  }
}



/*


Notice: Turning IAQ SEND off
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x23|0x5b|0xc1|0x10|0x03|
                        New Page | Status
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x2d|0x45|0x71|0x75|0x69|0x70|0x6d|0x65|0x6e|0x74|0x20|0x53|0x74|0x61|0x74|0x75|0x73|0x00|0xcc|0x10|0x03|
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x00|0x46|0x69|0x6c|0x74|0x65|0x72|0x20|0x50|0x75|0x6d|0x70|0x00|0x90|0x10|0x03|
                         Message | 0 | Filter Pump
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x01|0x53|0x6f|0x6c|0x61|0x72|0x20|0x48|0x65|0x61|0x74|0x20|0x45|0x4e|0x41|0x00|0x00|0x10|0x03|
                         Message | 1 | Solar Heat ENA
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x02|0x41|0x75|0x78|0x31|0x00|0xc9|0x10|0x03|
                         Message | 2 | Aux1
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x03|0x41|0x75|0x78|0x32|0x00|0xcb|0x10|0x03|
                         Message | 3 | Aux2
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x04|0x50|0x6f|0x6f|0x6c|0x20|0x4c|0x69|0x67|0x68|0x74|0x00|0x1e|0x10|0x03|
                         Message | 4 | Pool Light
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x05|0x41|0x75|0x78|0x36|0x00|0xd1|0x10|0x03|
                         Message | 5 | Aux6
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x06|0x4a|0x61|0x6e|0x64|0x79|0x20|0x65|0x50|0x55|0x4d|0x50|0x20|0x20|0x20|0x31|0x00|0xbc|0x10|0x03|
                         Message | 6 | Jandy ePUMP   1
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x07|0x20|0x20|0x20|0x20|0x52|0x50|0x4d|0x3a|0x20|0x32|0x37|0x35|0x30|0x00|0x06|0x10|0x03|
                         Message | 7 |     RPM: 2750
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x08|0x20|0x20|0x57|0x61|0x74|0x74|0x73|0x3a|0x20|0x30|0x00|0x4d|0x10|0x03|
                         Message | 8 |   Watts: 0
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x09|0x49|0x6e|0x74|0x65|0x6c|0x6c|0x69|0x66|0x6c|0x6f|0x20|0x56|0x46|0x20|0x32|0x00|0x91|0x10|0x03|
                         Message | 9 | Intelliflo VF 2
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x0a|0x20|0x20|0x20|0x20|0x52|0x50|0x4d|0x3a|0x20|0x32|0x32|0x35|0x30|0x00|0x04|0x10|0x03|
                         Message | 10 |     RPM: 2250
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x0b|0x20|0x20|0x57|0x61|0x74|0x74|0x73|0x3a|0x20|0x31|0x30|0x30|0x00|0xb1|0x10|0x03|
                         Message | 11 |   Watts: 100
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x0c|0x20|0x20|0x20|0x20|0x47|0x50|0x4d|0x3a|0x20|0x31|0x30|0x30|0x00|0xc3|0x10|0x03|
                         Message | 12 |     GPM: 100
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x0d|0x49|0x6e|0x74|0x65|0x6c|0x6c|0x69|0x66|0x6c|0x6f|0x20|0x56|0x53|0x20|0x33|0x00|0xa3|0x10|0x03|
                         Message | 13 | Intelliflo VS 3
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x0e|0x28|0x4f|0x66|0x66|0x6c|0x69|0x6e|0x65|0x29|0x00|0x8a|0x10|0x03|
                         Message | 14 | (Offline)
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x0f|0x20|0x00|0x97|0x10|0x03|
                         Message | 15 |  
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x10|0x20|0x00|0x98|0x10|0x03|
                         Message | 16 |  
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x11|0x20|0x00|0x99|0x10|0x03|
                         Message | 17 |  
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x24|0x01|0x00|0x00|0x00|0x00|0x00|0x68|0x10|0x03|
                          Button | 1 |  |-|  |-|  |-|  -off- 
Notice: Turning IAQ SEND on

*/

/*
Notice: Turning IAQ SEND off
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x23|0x5b|0xc1|0x10|0x03|
                        New Page | Status
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x2d|0x45|0x71|0x75|0x69|0x70|0x6d|0x65|0x6e|0x74|0x20|0x53|0x74|0x61|0x74|0x75|0x73|0x00|0xcc|0x10|0x03|
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x00|0x46|0x69|0x6c|0x74|0x65|0x72|0x20|0x50|0x75|0x6d|0x70|0x00|0x90|0x10|0x03|
                         Message | 0 | Filter Pump
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x01|0x41|0x75|0x78|0x31|0x00|0xc8|0x10|0x03|
                         Message | 1 | Aux1
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x02|0x41|0x75|0x78|0x32|0x00|0xca|0x10|0x03|
                         Message | 2 | Aux2
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x03|0x50|0x6f|0x6f|0x6c|0x20|0x4c|0x69|0x67|0x68|0x74|0x00|0x1d|0x10|0x03|
                         Message | 3 | Pool Light
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x04|0x41|0x75|0x78|0x36|0x00|0xd0|0x10|0x03|
                         Message | 4 | Aux6
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x05|0x4a|0x61|0x6e|0x64|0x79|0x20|0x65|0x50|0x55|0x4d|0x50|0x20|0x20|0x20|0x31|0x00|0xbb|0x10|0x03|
                         Message | 5 | Jandy ePUMP   1
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x06|0x20|0x20|0x20|0x20|0x52|0x50|0x4d|0x3a|0x20|0x32|0x37|0x35|0x30|0x00|0x05|0x10|0x03|
                         Message | 6 |     RPM: 2750
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x07|0x20|0x20|0x57|0x61|0x74|0x74|0x73|0x3a|0x20|0x30|0x00|0x4c|0x10|0x03|
                         Message | 7 |   Watts: 0
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x08|0x49|0x6e|0x74|0x65|0x6c|0x6c|0x69|0x66|0x6c|0x6f|0x20|0x56|0x46|0x20|0x32|0x00|0x90|0x10|0x03|
                         Message | 8 | Intelliflo VF 2
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x09|0x20|0x20|0x20|0x20|0x52|0x50|0x4d|0x3a|0x20|0x32|0x32|0x35|0x30|0x00|0x03|0x10|0x03|
                         Message | 9 |     RPM: 2250
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x0a|0x20|0x20|0x57|0x61|0x74|0x74|0x73|0x3a|0x20|0x31|0x30|0x33|0x00|0xb3|0x10|0x03|
                         Message | 10 |   Watts: 103
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x0b|0x20|0x20|0x20|0x20|0x47|0x50|0x4d|0x3a|0x20|0x31|0x30|0x30|0x00|0xc2|0x10|0x03|
                         Message | 11 |     GPM: 100
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x0c|0x49|0x6e|0x74|0x65|0x6c|0x6c|0x69|0x66|0x6c|0x6f|0x20|0x56|0x53|0x20|0x33|0x00|0xa2|0x10|0x03|
                         Message | 12 | Intelliflo VS 3
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x0d|0x20|0x20|0x20|0x20|0x52|0x50|0x4d|0x3a|0x20|0x31|0x37|0x35|0x30|0x00|0x0b|0x10|0x03|
                         Message | 13 |     RPM: 1750
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x0e|0x20|0x20|0x57|0x61|0x74|0x74|0x73|0x3a|0x20|0x33|0x34|0x00|0x8a|0x10|0x03|
                         Message | 14 |   Watts: 34
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x0f|0x41|0x51|0x55|0x41|0x50|0x55|0x52|0x45|0x20|0x33|0x30|0x25|0x00|0x83|0x10|0x03|
                         Message | 15 | AQUAPURE 30%
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x10|0x53|0x61|0x6c|0x74|0x20|0x33|0x38|0x30|0x30|0x20|0x50|0x50|0x4d|0x00|0x04|0x10|0x03|
                         Message | 16 | Salt 3800 PPM
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x25|0x11|0x20|0x00|0x99|0x10|0x03|
                         Message | 17 |  
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x24|0x01|0x00|0x00|0x00|0x00|0x00|0x68|0x10|0x03|
                          Button | 1 |  |-|  |-|  |-|  -off- 
Notice: Turning IAQ SEND on
*/


/*

Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x24|0x01|0x00|0x00|0x00|0x00|0x00|0x68|0x10|0x03|
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x24|0x01|0x00|0x00|0x02|0x00|0x00|0x6a|0x10|0x03|


No next page
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x24|0x01|0x00|0x00|0x00|0x00|0x00|0x68|0x10|0x03|
                          Button | 1 | 0x00 |  |-|  |-|  -off- 

Get next page
Debug:  To 0x00 of type      Ack | HEX: 0x10|0x02|0x00|0x01|0x00|0x12|0x25|0x10|0x03|

Has next page
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x24|0x01|0x00|0x00|0x02|0x00|0x00|0x6a|0x10|0x03|
                          Button | 1 | 0x02 |  |-|  |-|  -off- 

Can go up
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x24|0x01|0x00|0x00|0x00|0x00|0x00|0x68|0x10|0x03|
                          Button | 1 | 0x00 |  |-|  |-|  -off- 
*/

/*

No Next page
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x24|0x01|0x00|0x00|0x00|0x00|0x00|0x68|0x10|0x03|
                          Button | 1 | 0x00 |  |-|  |-|  -off- 
Next Page
Notice:    Jandy Packet | HEX: 0x10|0x02|0x31|0x24|0x01|0x00|0x00|0x02|0x00|0x00|0x6a|0x10|0x03|
                          Button | 1 | 0x02 |  |-|  |-|  -off- 


*/
