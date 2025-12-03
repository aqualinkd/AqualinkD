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
#include <stdbool.h>

#include "rs_devices.h"
#include "devices_jandy.h"
#include "aq_serial.h"
#include "aqualink.h"
#include "utils.h"
#include "aq_mqtt.h"
#include "packetLogger.h"
#include "iaqualink.h"

#include "json_messages.h"
/*
  All button errors
  'Check AQUAPURE No Flow'
  'Check AQUAPURE Low Salt'
  'Check AQUAPURE High Salt'
  'Check AQUAPURE General Fault'
*/

static int _swg_noreply_cnt = 0;

typedef enum heatpumpstate{
  HP_HEAT,
  HP_COOL,
  HP_UNKNOWN
} heatpumpstate;

typedef enum heatpumpmsgfrom{
  HP_TO_PANEL,
  HP_FROM_PANEL,
  HP_DISPLAY
} heatpumpmsgfrom;

void updateHeatPumpLed(heatpumpstate state, aqledstate ledstate, struct aqualinkdata *aqdata, heatpumpmsgfrom from);

bool shouldPrintJAndyDebugPacket()
{
  if (getLogLevel(DJAN_LOG) == LOG_DEBUG && getLogLevel(RSSD_LOG) < LOG_DEBUG ) {
    return true;
  }

  return false;
}
void printJandyDebugPacket (const char *msg, const unsigned char *packet, int packet_length)
{
  // Only log if we are jandy debug mode and not serial debug (otherwise it'll print twice)
  //if (getLogLevel(DJAN_LOG) == LOG_DEBUG && getLogLevel(RSSD_LOG) < LOG_DEBUG ) {
  if (shouldPrintJAndyDebugPacket()) {
    char frame[1024];
    //beautifyPacket(frame, 1024, packet, packet_length, true);

    sprintFrame(frame, 1024, packet, packet_length);
    LOG(DJAN_LOG, LOG_DEBUG, "%-4s %-6s: 0x%02hhx of type %16.16s | HEX: %s", 
                            (packet[PKT_DEST]==0x00?"From":"To"),
                            msg,
                            packet[PKT_DEST],
                            get_packet_type(packet, packet_length),
                            frame);
  }
}

bool processJandyPacket(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata)
{
  static rsDeviceType interestedInNextAck = DRS_NONE;
  static unsigned char previous_packet_to = NUL; // bad name, it's not previous, it's previous that we were interested in.
  int rtn = false;

  // We received the ack from a Jandy device we are interested in
  if (packet_buffer[PKT_DEST] == DEV_MASTER && interestedInNextAck != DRS_NONE)
  {
    if (interestedInNextAck == DRS_SWG)
    {
      printJandyDebugPacket("SWG", packet_buffer, packet_length);
      rtn = processPacketFromSWG(packet_buffer, packet_length, aqdata, previous_packet_to);
    }
    else if (interestedInNextAck == DRS_EPUMP)
    {
      printJandyDebugPacket("EPump", packet_buffer, packet_length);
      rtn = processPacketFromJandyPump(packet_buffer, packet_length, aqdata, previous_packet_to);
    }
    else if (interestedInNextAck == DRS_JXI)
    {
      printJandyDebugPacket("JXi", packet_buffer, packet_length);
      rtn = processPacketFromJandyJXiHeater(packet_buffer, packet_length, aqdata, previous_packet_to);
    }
    else if (interestedInNextAck == DRS_LX)
    {
      printJandyDebugPacket("LX", packet_buffer, packet_length);
      rtn = processPacketFromJandyLXHeater(packet_buffer, packet_length, aqdata, previous_packet_to);
    }
    else if (interestedInNextAck == DRS_CHEM_FEED)
    {
      printJandyDebugPacket("ChemL", packet_buffer, packet_length);
      rtn = processPacketFromJandyChemFeeder(packet_buffer, packet_length, aqdata, previous_packet_to);
    }
    else if (interestedInNextAck == DRS_CHEM_ANLZ)
    {
      printJandyDebugPacket("CemSnr", packet_buffer, packet_length);
      rtn = processPacketFromJandyChemAnalyzer(packet_buffer, packet_length, aqdata, previous_packet_to);
    }
    else if (interestedInNextAck == DRS_HEATPUMP)
    {
      printJandyDebugPacket("HPump", packet_buffer, packet_length);
      rtn = processPacketFromHeatPump(packet_buffer, packet_length, aqdata, previous_packet_to);
    }
    else if (interestedInNextAck == DRS_JLIGHT)
    {
      printJandyDebugPacket("JLight", packet_buffer, packet_length);
      rtn = processPacketFromJandyLight(packet_buffer, packet_length, aqdata, previous_packet_to);
    }
    interestedInNextAck = DRS_NONE;
    previous_packet_to = NUL;
  }
  // We were expecting an ack from Jandy device but didn't receive it.
  else if (packet_buffer[PKT_DEST] != DEV_MASTER && interestedInNextAck != DRS_NONE)
  {
    if (interestedInNextAck == DRS_SWG && aqdata->ar_swg_device_status != SWG_STATUS_OFF)
    { // SWG Offline
      processMissingAckPacketFromSWG(previous_packet_to, aqdata, previous_packet_to);
    }
    else if (interestedInNextAck == DRS_EPUMP)
    { // ePump offline
      processMissingAckPacketFromJandyPump(previous_packet_to, aqdata, previous_packet_to);
    }
    interestedInNextAck = DRS_NONE;
    previous_packet_to = NUL;
  }
  else if (READ_RSDEV_SWG && is_swg_id(packet_buffer[PKT_DEST]))
  {
    interestedInNextAck = DRS_SWG;
    printJandyDebugPacket("SWG", packet_buffer, packet_length);
    rtn = processPacketToSWG(packet_buffer, packet_length, aqdata/*, _aqconfig_.swg_zero_ignore*/);
    previous_packet_to = packet_buffer[PKT_DEST];
  }
  else if (READ_RSDEV_ePUMP && is_jandy_pump_id(packet_buffer[PKT_DEST]))
  {
    interestedInNextAck = DRS_EPUMP;
    printJandyDebugPacket("EPump", packet_buffer, packet_length);
    rtn = processPacketToJandyPump(packet_buffer, packet_length, aqdata);
    previous_packet_to = packet_buffer[PKT_DEST];
  }
  else if (READ_RSDEV_JXI && is_jxi_heater_id(packet_buffer[PKT_DEST]))
  {
    interestedInNextAck = DRS_JXI;
    printJandyDebugPacket("JXi", packet_buffer, packet_length);
    rtn = processPacketToJandyJXiHeater(packet_buffer, packet_length, aqdata);
    previous_packet_to = packet_buffer[PKT_DEST];
  }
  else if (READ_RSDEV_LX && is_lx_heater_id(packet_buffer[PKT_DEST]))
  {
    interestedInNextAck = DRS_LX;
    printJandyDebugPacket("LX", packet_buffer, packet_length);
    rtn = processPacketToJandyLXHeater(packet_buffer, packet_length, aqdata);
    previous_packet_to = packet_buffer[PKT_DEST];
  }
  else if (READ_RSDEV_CHEM_FEDR && is_chem_feeder_id(packet_buffer[PKT_DEST] ))
  {
    interestedInNextAck = DRS_CHEM_FEED;
    printJandyDebugPacket("ChemL", packet_buffer, packet_length);
    rtn = processPacketToJandyChemFeeder(packet_buffer, packet_length, aqdata);
    previous_packet_to = packet_buffer[PKT_DEST];
  }
  else if (READ_RSDEV_CHEM_ANLZ && is_chem_anlzer_id(packet_buffer[PKT_DEST]))
  {
    interestedInNextAck = DRS_CHEM_ANLZ;
    printJandyDebugPacket("CemSnr", packet_buffer, packet_length);
    rtn = processPacketToJandyChemAnalyzer(packet_buffer, packet_length, aqdata);
    previous_packet_to = packet_buffer[PKT_DEST];
  }
  else if (READ_RSDEV_iAQLNK && is_aqualink_touch_id(packet_buffer[PKT_DEST]) // should we add is_iaqualink_id() as well????
          && packet_buffer[PKT_DEST] != _aqconfig_.extended_device_id) // We would have already read extended_device_id frame
  {
    process_iAqualinkStatusPacket(packet_buffer, packet_length, aqdata);
  }
  else if (READ_RSDEV_HPUMP && is_heat_pump_id(packet_buffer[PKT_DEST] ))
  {
    interestedInNextAck = DRS_HEATPUMP;
    printJandyDebugPacket("HPump", packet_buffer, packet_length);
    rtn = processPacketToHeatPump(packet_buffer, packet_length, aqdata);
    previous_packet_to = packet_buffer[PKT_DEST];
  }
  else if (READ_RSDEV_JLIGHT && is_jandy_light_id(packet_buffer[PKT_DEST]))
  {
    interestedInNextAck = DRS_JLIGHT;
    printJandyDebugPacket("JLight", packet_buffer, packet_length);
    rtn = processPacketToJandyLight(packet_buffer, packet_length, aqdata);
    previous_packet_to = packet_buffer[PKT_DEST];
  }
  else
  {
    interestedInNextAck = DRS_NONE;
    previous_packet_to = NUL;
  }
/*
  if (packet_buffer[PKT_CMD] != CMD_PROBE && getLogLevel(DJAN_LOG) >= LOG_DEBUG) {
    char msg[1000];
    beautifyPacket(msg, packet_buffer, packet_length, true);
    LOG(DJAN_LOG, LOG_DEBUG, "Jandy : %s\n", msg);
  }
*/
  return rtn;
}







bool processPacketToSWG(unsigned char *packet, int packet_length, struct aqualinkdata *aqdata /*, int swg_zero_ignore*/) {
  //static int swg_zero_cnt = 0;
  bool changedAnything = false;


  // Only read message from controller to SWG to set SWG Percent if we are not programming, as we might be changing this
  if (packet[3] == CMD_PERCENT && aqdata->active_thread.thread_id == 0 && packet[4] != 0xFF) {
    // In service or timeout mode SWG set % message is very strange. AR %% | HEX: 0x10|0x02|0x50|0x11|0xff|0x72|0x10|0x03|
    // Not really sure what to do with this, just ignore 0xff / 255 for the moment. (if statment above)

      if (aqdata->swg_percent != (int)packet[4]) {
        //aqdata->swg_percent = (int)packet[4];
        setSWGpercent(aqdata, (int)packet[4]);
        changedAnything = true;
        SET_DIRTY(aqdata->is_dirty);
        LOG(DJAN_LOG, LOG_INFO, "Set SWG %% to %d from control panel to SWG\n", aqdata->swg_percent);
      }

    if (aqdata->swg_percent > 100)
      SET_IF_CHANGED(aqdata->boost, true, aqdata->is_dirty);
    else
      SET_IF_CHANGED(aqdata->boost, false, aqdata->is_dirty);
  }
  return changedAnything;
}

unsigned char _SWG_ID = NUL;

bool processPacketFromSWG(unsigned char *packet, int packet_length, struct aqualinkdata *aqdata, const unsigned char previous_packet_to) {
  bool changedAnything = false;
  _swg_noreply_cnt = 0;

  // Capture the SWG ID.  We could have more than one, but for the moment AqualinkD only supports one so we'll pick the first one.
  if (_SWG_ID == NUL) {
    _SWG_ID = previous_packet_to;
  } else if (_SWG_ID != NUL && _SWG_ID != previous_packet_to) {
    LOG(DJAN_LOG, LOG_WARNING, "We have two SWG, AqualinkD only supports one. using ID 0x%02hhx, ignoring 0x%02hhx\n", _SWG_ID, previous_packet_to);
    return changedAnything;
  }

  if (packet[PKT_CMD] == CMD_PPM) {
    //aqdata->ar_swg_device_status = packet[5];
    setSWGdeviceStatus(aqdata, JANDY_DEVICE, packet[5]);
    if (aqdata->swg_delayed_percent != TEMP_UNKNOWN && aqdata->ar_swg_device_status == SWG_STATUS_ON) { // We have a delayed % to set.
      char sval[10];
      snprintf(sval, 9, "%d", aqdata->swg_delayed_percent);
#ifdef NEW_AQ_PROGRAMMER
      aq_programmer(AQ_SET_SWG_PERCENT, NULL, aqdata->swg_delayed_percent, AQP_NULL, aqdata);
#else
      aq_programmer(AQ_SET_SWG_PERCENT, sval, aqdata);
#endif
      LOG(DJAN_LOG, LOG_NOTICE, "Setting SWG %% to %d, from delayed message\n", aqdata->swg_delayed_percent);
      aqdata->swg_delayed_percent = TEMP_UNKNOWN;
    }

    if ( (packet[4] * 100) != aqdata->swg_ppm ) {
      aqdata->swg_ppm = packet[4] * 100;
      LOG(DJAN_LOG, LOG_INFO, "Received SWG PPM %d from SWG packet\n", aqdata->swg_ppm);
      changedAnything = true;
      SET_DIRTY(aqdata->is_dirty);
    }
    // LOG(LOG_DEBUG, "Read SWG PPM %d from ID 0x%02hhx\n", aqdata.swg_ppm, SWG_DEV_ID);
  }

  return changedAnything;
}

void processMissingAckPacketFromSWG(unsigned char destination, struct aqualinkdata *aqdata, const unsigned char previous_packet_to)
{
  // SWG_STATUS_UNKNOWN means we have never seen anything from SWG, so leave as is. 
  // IAQTOUCH & ONETOUCH give us AQUAPURE=0 but ALLBUTTON doesn't, so only turn off if we are not in extra device mode.
  // NSF Need to check that we actually use 0 from IAQTOUCH & ONETOUCH
  if (_SWG_ID != previous_packet_to) {
    //LOG(DJAN_LOG, LOG_DEBUG, "Ignoring SWG no reply from 0x%02hhx\n", previous_packet_to);
    return;
  }

  if ( aqdata->ar_swg_device_status != SWG_STATUS_UNKNOWN && isIAQT_ENABLED == false && isONET_ENABLED == false )
  { 
    if ( _swg_noreply_cnt < 3 ) {
              //_aqualink_data.ar_swg_device_status = SWG_STATUS_OFF;
              //_aqualink_data.updated = true;
      setSWGoff(aqdata);
      _swg_noreply_cnt++; // Don't put in if, as it'll go past size limit
    }
  }     
}

bool isSWGDeviceErrorState(unsigned char status)
{
  if (status == SWG_STATUS_NO_FLOW ||
      status == SWG_STATUS_CHECK_PCB ||
      status == SWG_STATUS_LOW_TEMP ||
      status == SWG_STATUS_HIGH_CURRENT ||
      status == SWG_STATUS_NO_FLOW)
      // Maybe add CLEAN_CELL and GENFAULT here
    return true;
  else
    return false;
}

void setSWGdeviceStatus(struct aqualinkdata *aqdata, emulation_type requester, unsigned char status) {
  static unsigned char last_status = SWG_STATUS_UNKNOWN;
  /* This is only needed for DEBUG
  static bool haveSeenRSSWG = false;

  if (requester == JANDY_DEVICE) {
    haveSeenRSSWG = true;
  }
  */
   // If we are reading state directly from RS458, then ignore everything else.
  //if ( READ_RSDEV_SWG && requester != JANDY_DEVICE ) {
  //  return;
  //}

  //LOG(DJAN_LOG, LOG_DEBUG, "Set SWG device state to '0x%02hhx', request from %d\n", aqdata->ar_swg_device_status, requester);

  
  if ((aqdata->ar_swg_device_status == status) || (last_status == status)) {
    LOG(DJAN_LOG, LOG_DEBUG, "Set SWG device state to '0x%02hhx', request from %d (no change)\n", aqdata->ar_swg_device_status, requester);
    return;
  }
  last_status = status;

  // If we get (ALLBUTTON, SWG_STATUS_CHECK_PCB // GENFAULT), it sends this for many status, like clean cell.
  // So if we are in one of those states, don't use it.

  // Need to rethink this.  Use general fault only if we are not reading SWG status direct from device
  //if ( READ_RSDEV_SWG && requester == ALLBUTTON && status == SWG_STATUS_GENFAULT ) {

  // SWG_STATUS_GENFAULT is shown on panels for many reasons, if we are NOT reading the status directly from the SWG
  // then use it, otherwise disguard it as we will have a better status
  if (requester == ALLBUTTON && status == SWG_STATUS_GENFAULT ) {
    if (aqdata->ar_swg_device_status > SWG_STATUS_ON && 
        aqdata->ar_swg_device_status < SWG_STATUS_TURNING_OFF) {
          LOG(DJAN_LOG, LOG_DEBUG, "Ignoring set SWG device state to '0x%02hhx', request from %d\n", aqdata->ar_swg_device_status, requester);
          return;
        }
  }

  // Check validity of status and set as appropiate
  switch (status) {

  case SWG_STATUS_ON:
  case SWG_STATUS_NO_FLOW:
  case SWG_STATUS_LOW_SALT:
  case SWG_STATUS_HI_SALT:
  case SWG_STATUS_HIGH_CURRENT:
  case SWG_STATUS_CLEAN_CELL:
  case SWG_STATUS_LOW_VOLTS:
  case SWG_STATUS_LOW_TEMP:
  case SWG_STATUS_CHECK_PCB:
  case SWG_STATUS_GENFAULT:
    SET_IF_CHANGED(aqdata->ar_swg_device_status, status, aqdata->is_dirty);
    SET_IF_CHANGED(aqdata->swg_led_state, (isSWGDeviceErrorState(status)?ENABLE:ON), aqdata->is_dirty);
    break;
  case SWG_STATUS_OFF: // THIS IS OUR OFF STATUS, NOT AQUAPURE
  case SWG_STATUS_TURNING_OFF:
    SET_IF_CHANGED(aqdata->ar_swg_device_status, status, aqdata->is_dirty);
    SET_IF_CHANGED(aqdata->swg_led_state, OFF, aqdata->is_dirty);
    break;
  default:
    LOG(DJAN_LOG, LOG_WARNING, "Ignoring set SWG device to state '0x%02hhx', state is unknown\n", status);
    return;
    break;
  }
//printf("************ Set SWG device state to '0x%02hhx', request from %d, LED state = %d\n", aqdata->ar_swg_device_status, requester, aqdata->swg_led_state);
  LOG(DJAN_LOG, LOG_DEBUG, "Set SWG device state to '0x%02hhx', request from %d, LED state = %d\n", aqdata->ar_swg_device_status, requester, aqdata->swg_led_state);
}


/*
bool updateSWG(struct aqualinkdata *aqdata, emulation_type requester, aqledstate state, int percent)
{
  switch (requester) {
    case ALLBUTTON: // no insight into 0% (just blank)
    break;
    case ONETOUCH:
    break;
    case IAQTOUCH:
    break;
    case AQUAPDA:
    break;
    case JANDY_DEVICE:
    break;
  }
}
*/

bool setSWGboost(struct aqualinkdata *aqdata, bool on) {
  if (!on) {
    SET_IF_CHANGED(aqdata->boost, false, aqdata->is_dirty);
    SET_IF_CHANGED_STRCPY(aqdata->boost_msg, "", aqdata->is_dirty);
    SET_IF_CHANGED(aqdata->swg_percent, 0, aqdata->is_dirty);
  } else {
    SET_IF_CHANGED(aqdata->boost, true, aqdata->is_dirty);
    SET_IF_CHANGED(aqdata->swg_percent, 101, aqdata->is_dirty);
    SET_IF_CHANGED(aqdata->swg_led_state, ON, aqdata->is_dirty);
  }

  return true;
}

// Only change SWG percent if we are not in SWG programming
bool changeSWGpercent(struct aqualinkdata *aqdata, int percent) {
  
  if (in_swg_programming_mode(aqdata)) {
    LOG(DJAN_LOG, LOG_DEBUG, "Ignoring set SWG %% to %d due to programming SWG\n", aqdata->swg_percent);
    return false;
  }

  setSWGpercent(aqdata, percent);
  return true;
}

void setSWGoff(struct aqualinkdata *aqdata) {

  SET_IF_CHANGED(aqdata->ar_swg_device_status, SWG_STATUS_OFF, aqdata->is_dirty);
  SET_IF_CHANGED(aqdata->swg_led_state, OFF, aqdata->is_dirty);

  LOG(DJAN_LOG, LOG_DEBUG, "Set SWG to off\n");
}

void setSWGenabled(struct aqualinkdata *aqdata) {
  if (aqdata->swg_led_state != ENABLE) {
    SET_IF_CHANGED(aqdata->swg_led_state, ENABLE, aqdata->is_dirty);
    LOG(DJAN_LOG, LOG_DEBUG, "Set SWG to Enable\n");
  }
  //SET_IF_CHANGED(aqdata->swg_led_state, ENABLE, aqdata->is_dirty);
}

// force a Change SWG percent.
void setSWGpercent(struct aqualinkdata *aqdata, int percent) {
 
  SET_IF_CHANGED(aqdata->swg_percent, percent, aqdata->is_dirty);

  if (aqdata->swg_percent > 0) {
    //LOG(DJAN_LOG, LOG_DEBUG, "swg_led_state=%d, swg_led_state=%d, isSWGDeviceErrorState=%d, ar_swg_device_status=%d\n",aqdata->swg_led_state, aqdata->swg_led_state, isSWGDeviceErrorState(aqdata->ar_swg_device_status),aqdata->ar_swg_device_status);
    if (aqdata->swg_led_state == OFF || (aqdata->swg_led_state == ENABLE && ! isSWGDeviceErrorState(aqdata->ar_swg_device_status)) ) // Don't change ENABLE / FLASH
      SET_IF_CHANGED(aqdata->swg_led_state, ON, aqdata->is_dirty);
    
    if (aqdata->ar_swg_device_status == SWG_STATUS_UNKNOWN) 
      SET_IF_CHANGED(aqdata->ar_swg_device_status, SWG_STATUS_ON, aqdata->is_dirty); 
    
    if (aqdata->swg_led_state == LED_S_UNKNOWN)
      SET_IF_CHANGED(aqdata->swg_led_state, ON, aqdata->is_dirty);
  
  } if ( aqdata->swg_percent == 0 ) {
    if (aqdata->swg_led_state == ON)
      SET_IF_CHANGED(aqdata->swg_led_state, ENABLE, aqdata->is_dirty); // Don't change OFF 
    
    if (aqdata->ar_swg_device_status == SWG_STATUS_UNKNOWN)
      SET_IF_CHANGED(aqdata->ar_swg_device_status, SWG_STATUS_ON, aqdata->is_dirty); // Maybe this should be off

    if (aqdata->swg_led_state == LED_S_UNKNOWN)
      SET_IF_CHANGED(aqdata->swg_led_state, ENABLE, aqdata->is_dirty);
  
  }

  LOG(DJAN_LOG, LOG_DEBUG, "Set SWG %% to %d, LED=%d, FullStatus=0x%02hhx\n", aqdata->swg_percent, aqdata->swg_led_state, aqdata->ar_swg_device_status);
}

aqledstate get_swg_led_state(struct aqualinkdata *aqdata)
{
  switch (aqdata->ar_swg_device_status) {
  
  case SWG_STATUS_ON:
    return (aqdata->swg_percent > 0?ON:ENABLE);
    break;
  case SWG_STATUS_NO_FLOW:
    return ENABLE;
    break;
  case SWG_STATUS_LOW_SALT:
    return (aqdata->swg_percent > 0?ON:ENABLE);
    break;
  case SWG_STATUS_HI_SALT:
    return (aqdata->swg_percent > 0?ON:ENABLE);
    break;
  case SWG_STATUS_HIGH_CURRENT:
    return (aqdata->swg_percent > 0?ON:ENABLE);
    break;
  case SWG_STATUS_TURNING_OFF:
    return OFF;
    break;
  case SWG_STATUS_CLEAN_CELL:
    return (aqdata->swg_percent > 0?ON:ENABLE);
      return ENABLE;
    break;
  case SWG_STATUS_LOW_VOLTS:
    return ENABLE;
    break;
  case SWG_STATUS_LOW_TEMP:
    return ENABLE;
    break;
  case SWG_STATUS_CHECK_PCB:
    return ENABLE;
    break;
  case SWG_STATUS_OFF: // THIS IS OUR OFF STATUS, NOT AQUAPURE
    return OFF;
    break;
  case SWG_STATUS_GENFAULT:
    return ENABLE;
  break;
  default:
    return (aqdata->swg_percent > 0?ON:ENABLE);
    break;
  }
}

const char *get_swg_status_msg(struct aqualinkdata *aqdata)
{
  switch (aqdata->ar_swg_device_status) {

  case SWG_STATUS_ON:
    return "AQUAPURE GENERATING CHLORINE";
    break;
  case SWG_STATUS_NO_FLOW:
    return "AQUAPURE NO FLOW";
    break;
  case SWG_STATUS_LOW_SALT:
    return "AQUAPURE LOW SALT";
    break;
  case SWG_STATUS_HI_SALT:
    return "AQUAPURE HIGH SALT";
    break;
  case SWG_STATUS_HIGH_CURRENT:
    return "AQUAPURE HIGH CURRENT";
    break;
  case SWG_STATUS_TURNING_OFF:
    return "AQUAPURE TURNING OFF";
    break;
  case SWG_STATUS_CLEAN_CELL:
    return "AQUAPURE CLEAN CELL";
    break;
  case SWG_STATUS_LOW_VOLTS:
    return "AQUAPURE LOW VOLTAGE";
    break;
  case SWG_STATUS_LOW_TEMP:
    return "AQUAPURE WATER TEMP LOW";
    break;
  case SWG_STATUS_CHECK_PCB:
    return "AQUAPURE CHECK PCB";
    break;
  case SWG_STATUS_OFF: // THIS IS OUR OFF STATUS, NOT AQUAPURE
    return "AQUAPURE OFF";
    break;
  case SWG_STATUS_GENFAULT:
    return "AQUAPURE GENERAL FAULT";
    break;
  default:
    return "AQUAPURE UNKNOWN STATUS";
    break;
  }
}



#define EP_HI_B_WAT 8
#define EP_LO_B_WAT 7
#define EP_HI_B_RPM 7
#define EP_LO_B_RPM 6

bool processPacketToJandyPump(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata)
{
  /*
  Set & Sataus Watts.  Looks like send to ePump type 0x45, return type 0xf1|0x45
  JandyDvce: To   ePump:  Read To 0x78 of type   Unknown '0x45' | HEX: 0x10|0x02|0x78|0x45|0x00|0x05|0xd4|0x10|0x03|
  JandyDvce: From ePump:  Read To 0x00 of type   Unknown '0x1f' | HEX: 0x10|0x02|0x00|0x1f|0x45|0x00|0x05|0x1d|0x05|0x9d|0x10|0x03|
  JandyDvce: From ePump:  Read To 0x00 of type   Unknown '0x1f' | HEX: 0x10|0x02|0x00|0x1f|  69|   0|   5|  29|   5|0x9d|0x10|0x03| (Decimal)
  Type 0x1F and cmd 0x45 is Watts = 5 * (256) + 29 = 1309  or  Byte 8 * 265 + Byte 7
  */
 /*
  Set & Sataus RPM.  Looks like send to ePump type 0x44, return type 0xf1|0x44
  JandyDvce: To   ePump:  Read To 0x78 of type   Unknown '0x44' | HEX: 0x10|0x02|0x78|0x44|0x00|0x60|0x27|0x55|0x10|0x03|  
  JandyDvce: From ePump:  Read To 0x00 of type   Unknown '0x1f' | HEX: 0x10|0x02|0x00|0x1f|0x44|0x00|0x60|0x27|0x00|0xfc|0x10|0x03|
  JandyDvce: From ePump:  Read To 0x00 of type   Unknown '0x1f' | HEX: 0x10|0x02|0x00|0x1f|  68|   0|  96|  39|   0|0xfc|0x10|0x03| (Decimal)
  PDA:       PDA Menu Line 3 = SET TO 2520 RPM 

  Type 0x1F and cmd 0x45 is RPM = 39 * (256) + 96 / 4 = 2520  or  Byte 8 * 265 + Byte 7 / 4
 */

  // If type 0x45 and 0x44 set to interested in next command.
  if (packet_buffer[3] == CMD_EPUMP_RPM) {
    // All we need to do is set we are interested in next packet, but ca   lling function already did this.
    LOG(DJAN_LOG, LOG_INFO, "ControlPanel request Pump ID 0x%02hhx set RPM to %d\n",packet_buffer[PKT_DEST], ( (packet_buffer[EP_HI_B_RPM-1] * 256) + packet_buffer[EP_LO_B_RPM-1]) / 4 );
  } else if (packet_buffer[3] == CMD_EPUMP_WATTS) {
    LOG(DJAN_LOG, LOG_INFO, "ControlPanel request Pump ID 0x%02hhx get watts\n",packet_buffer[PKT_DEST]);
  }
    
  if (getLogLevel(DJAN_LOG) == LOG_DEBUG) {
  //find pump for message
    for (int i=0; i < aqdata->num_pumps; i++) {
      if (aqdata->pumps[i].pumpID == packet_buffer[PKT_DEST]) {
        LOG(DJAN_LOG, LOG_DEBUG, "Last panel info RPM:%d GPM:%d WATTS:%d\n", aqdata->pumps[i].rpm, aqdata->pumps[i].gpm, aqdata->pumps[i].watts);
        break;
      }
    }
  }

  return false;
}

bool processPacketFromJandyPump(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata, const unsigned char previous_packet_to)
{
  bool found=false;

  if (packet_buffer[3] == CMD_EPUMP_STATUS && packet_buffer[4] == CMD_EPUMP_RPM) {
    for (int i = 0; i < MAX_PUMPS; i++) {
      if ( aqdata->pumps[i].prclType == JANDY && aqdata->pumps[i].pumpID == previous_packet_to ) {
        LOG(DJAN_LOG, LOG_INFO, "Jandy Pump Status message = RPM %d\n",( (packet_buffer[EP_HI_B_RPM] * 256) + packet_buffer[EP_LO_B_RPM]) / 4 );
        //aqdata->pumps[i].rpm = ( (packet_buffer[EP_HI_B_RPM] * 256) + packet_buffer[EP_LO_B_RPM] ) / 4;
        SET_IF_CHANGED(aqdata->pumps[i].rpm, ( (packet_buffer[EP_HI_B_RPM] * 256) + packet_buffer[EP_LO_B_RPM] ) / 4, aqdata->is_dirty);
        found=true;
      }
    }
  } else if (packet_buffer[3] == CMD_EPUMP_STATUS && packet_buffer[4] == CMD_EPUMP_WATTS) {
    for (int i = 0; i < MAX_PUMPS; i++) {
      if ( aqdata->pumps[i].prclType == JANDY && aqdata->pumps[i].pumpID == previous_packet_to ) {
        LOG(DJAN_LOG, LOG_INFO, "Jandy Pump Status message = WATTS %d\n", (packet_buffer[EP_HI_B_WAT] * 256) + packet_buffer[EP_LO_B_WAT]);
        //aqdata->pumps[i].watts = (packet_buffer[EP_HI_B_WAT] * 256) + packet_buffer[EP_LO_B_WAT];
        SET_IF_CHANGED(aqdata->pumps[i].watts, (packet_buffer[EP_HI_B_WAT] * 256) + packet_buffer[EP_LO_B_WAT], aqdata->is_dirty);
        found=true;
      }
    }
  }

  if (!found) {
    if (packet_buffer[4] == CMD_EPUMP_RPM)
      LOG(DJAN_LOG, LOG_NOTICE, "Jandy Pump found at ID 0x%02hhx with RPM %d, but not configured, information ignored!\n",previous_packet_to,( (packet_buffer[EP_HI_B_RPM] * 256) + packet_buffer[EP_LO_B_RPM]) / 4 );
    else if (packet_buffer[4] == CMD_EPUMP_WATTS)
      LOG(DJAN_LOG, LOG_NOTICE, "Jandy Pump found at ID 0x%02hhx with WATTS %d, but not configured, information ignored!\n",previous_packet_to, (packet_buffer[EP_HI_B_WAT] * 256) + packet_buffer[EP_LO_B_WAT]);
  }

  
  return false;
}

void processMissingAckPacketFromJandyPump(unsigned char destination, struct aqualinkdata *aqdata, const unsigned char previous_packet_to)
{
  // Do nothing for the moment.
  return;
}

int getPumpStatus(int pumpIndex, struct aqualinkdata *aqdata)
{
  int rtn = aqdata->pumps[pumpIndex].pStatus;

  // look at notes in device_jandy for pump status.
  // 1 & 0 are on off, anything else error. So if below 1 use the panel status (pStatus).
  // Not set would be -999 TEMP_UNKNOWN
  // At moment Jandy pumps wil only have panel status, RS485 not decoded.
  if ( aqdata->pumps[pumpIndex].status > 1 ) {
    rtn = aqdata->pumps[pumpIndex].status;
  }

  return rtn;
}






bool processPacketToJandyJXiHeater(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata)
{

  if (packet_buffer[3] != CMD_JXI_PING) {
    // Not sure what this message is, so ignore
    // Maybe print a messsage.
    return false;
  }
  /*
  Below counfing first as bit 0
  4th bit 0x00 no pump on (nothing)
        0x10 seems to be JXi came online. nothing more
        0x11 (pool mode)
        0x12 (spa mode)
        0x19 heat pool
        0x1a heat spa
  5th bit 0x55 = 85 deg. (current pool setpoint)
  6th bit 0x66 = 102 deg. (current spa setpoint)
  7th bit 0x4f = current water temp 79 (0xFF is off / 255)
  */
 
  if (packet_buffer[5] != aqdata->pool_htr_set_point) {
    LOG(DJAN_LOG, LOG_INFO, "JXi pool setpoint %d, Pool heater sp %d (changing to LXi)\n", packet_buffer[5], aqdata->pool_htr_set_point);
    SET_IF_CHANGED(aqdata->pool_htr_set_point, packet_buffer[5], aqdata->is_dirty);
  }

  if (packet_buffer[6] != aqdata->spa_htr_set_point) {
    LOG(DJAN_LOG, LOG_INFO, "JXi spa setpoint %d, Spa heater sp %d (changing to LXi)\n", packet_buffer[6], aqdata->spa_htr_set_point);
    SET_IF_CHANGED(aqdata->spa_htr_set_point, packet_buffer[6], aqdata->is_dirty);
  }

  if (packet_buffer[7] != 0xff && packet_buffer[4] != 0x00) {
    if (packet_buffer[4] == 0x11 || packet_buffer[4] == 0x19) {
      if (aqdata->pool_temp != packet_buffer[7]) {
        LOG(DJAN_LOG, LOG_INFO, "JXi pool water temp %d, pool water temp %d (changing to LXi)\n", packet_buffer[7], aqdata->pool_temp);
        SET_IF_CHANGED(aqdata->pool_temp, packet_buffer[7], aqdata->is_dirty);
      }
    } else if (packet_buffer[4] == 0x12 || packet_buffer[4] == 0x1a) {
      if (aqdata->spa_temp != packet_buffer[7]) {
        LOG(DJAN_LOG, LOG_INFO, "JXi spa water temp %d, spa water temp %d (changing to LXi)\n", packet_buffer[7], aqdata->spa_temp);
        SET_IF_CHANGED(aqdata->spa_temp, packet_buffer[7], aqdata->is_dirty);
      }
    }
  }

  switch (packet_buffer[4]) {
    case 0x11:  // Pool heat off or enabled
    break;
    case 0x12:  // Pool Heat enabled or heating
    break;
    case 0x19:  // Spa heat off or enabled
    break;
    case 0x1a:  // Spa Hear Heat enabled or heating
    break;
  }

  /*
  char msg[1000];
  int length = 0;

  beautifyPacket(msg, packet_buffer, packet_length, true);
  LOG(DJAN_LOG, LOG_INFO, "To   JXi Heater: %s\n", msg);

  length += sprintf(msg+length, "Last panel info ");

  for (int i=0; i < aqdata->total_buttons; i++) 
  {
    if ( strcmp(BTN_POOL_HTR,aqdata->aqbuttons[i].name) == 0) {
      length += sprintf(msg+length, ", Pool Heat LED=%d ",aqdata->aqbuttons[i].led->state);
    }
    if ( strcmp(BTN_SPA_HTR,aqdata->aqbuttons[i].name) == 0) {
      length += sprintf(msg+length, ", Spa Heat LED=%d ",aqdata->aqbuttons[i].led->state);
    }
  }

  length += sprintf(msg+length, ", Pool SP=%d, Spa SP=%d",aqdata->pool_htr_set_point, aqdata->spa_htr_set_point);
  length += sprintf(msg+length, ", Pool temp=%d, Spa temp=%d",aqdata->pool_temp, aqdata->spa_temp);

  LOG(DJAN_LOG, LOG_INFO, "%s\n", msg);
  
  return false;
  */
   
  return true;
}

void getJandyHeaterError(struct aqualinkdata *aqdata, char *message) 
{
  if (aqdata->heater_err_status == NUL) {
    return;
  }

  int size = sprintf(message, "JXi Heater ");
  getJandyHeaterErrorMQTT(aqdata, message+size);
}

void getJandyHeaterErrorMQTT(struct aqualinkdata *aqdata, char *message) 
{
  switch (aqdata->heater_err_status) {
      case 0x00:
        //sprintf(message,  "");
      break;
      case 0x10:
        sprintf(message,  "FAULT HIGH LIMIT");
      break;
      case 0x02:
        sprintf(message,  "FAULT H20 SENSOR");
      break;
      case 0x08:
        sprintf(message,  "FAULT AUX MONITOR");
      break;
      default:
      //
      /*  Error we haven't decoded yet
       ?x?? check flow
       0x10 Fault high limit
       ?x?? Fault High Flu temp
       ?x?? Fault Check Igntion Control
       0x02 Fault Short H20 sensor (or Fault open water sensor)
       ?x?? Pump fault
       0x08 AUX Monitor
      */
        sprintf(message,  "FAULT 0x%02hhx",aqdata->heater_err_status);
      break;
    } 
}

bool processPacketFromJandyJXiHeater(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata, const unsigned char previous_packet_to)
{

  if (packet_buffer[3] != CMD_JXI_STATUS) {
    // Not sure what this message is, so ignore
    // Maybe print a messsage.
    return false;
  }
  
  // No error is 0x00, so blindly set it.
  aqdata->heater_err_status = packet_buffer[6];
   // Check if error first
  if (packet_buffer[6] != 0x00) {
    
  } else if (packet_buffer[4] == 0x00) {
    // Not heating.
    // Heater off or enabeled
  } else if (packet_buffer[4] == 0x08) {
    // Heating
    // Heater on of enabled
  }
  /*
  char msg[1000];
  int length = 0;   

  beautifyPacket(msg, packet_buffer, packet_length, true);
  LOG(DJAN_LOG, LOG_INFO, "From JXi Heater: %s\n", msg);

  length += sprintf(msg+length, "Last panel info ");

  for (int i=0; i < aqdata->total_buttons; i++) 
  {
    if ( strcmp(BTN_POOL_HTR,aqdata->aqbuttons[i].name) == 0) {
      length += sprintf(msg+length, ", Pool Heat LED=%d ",aqdata->aqbuttons[i].led->state);
    }
    if ( strcmp(BTN_SPA_HTR,aqdata->aqbuttons[i].name) == 0) {
      length += sprintf(msg+length, ", Spa Heat LED=%d ",aqdata->aqbuttons[i].led->state);
    }
  }

  length += sprintf(msg+length, ", Pool SP=%d, Spa SP=%d",aqdata->pool_htr_set_point, aqdata->spa_htr_set_point);
  length += sprintf(msg+length, ", Pool temp=%d, Spa temp=%d",aqdata->pool_temp, aqdata->spa_temp);

  LOG(DJAN_LOG, LOG_INFO, "%s\n", msg);

  return false;
  */
 return true;
}

bool processPacketToJandyLXHeater(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata)
{
  
  char msg[1024];
  int length = 0;

  length += sprintf(msg+length, "Last panel info ");

  for (int i=0; i < aqdata->total_buttons; i++) 
  {
    if ( strcmp(BTN_POOL_HTR,aqdata->aqbuttons[i].name) == 0) {
      length += sprintf(msg+length, ", Pool Heat LED=%d ",aqdata->aqbuttons[i].led->state);
    }
    if ( strcmp(BTN_SPA_HTR,aqdata->aqbuttons[i].name) == 0) {
      length += sprintf(msg+length, ", Spa Heat LED=%d ",aqdata->aqbuttons[i].led->state);
    }
  }

  length += sprintf(msg+length, ", Pool SP=%d, Spa SP=%d",aqdata->pool_htr_set_point, aqdata->spa_htr_set_point);
  length += sprintf(msg+length, ", Pool temp=%d, Spa temp=%d",aqdata->pool_temp, aqdata->spa_temp);

  LOG(DJAN_LOG, LOG_INFO, "%s\n", msg);

  return false;
  
}

bool processPacketFromJandyLXHeater(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata, const unsigned char previous_packet_to)
{
  char msg[1024];
  int length = 0;   

  length += sprintf(msg+length, "Last panel info ");

  for (int i=0; i < aqdata->total_buttons; i++) 
  {
    if ( strcmp(BTN_POOL_HTR,aqdata->aqbuttons[i].name) == 0) {
      length += sprintf(msg+length, ", Pool Heat LED=%d ",aqdata->aqbuttons[i].led->state);
    }
    if ( strcmp(BTN_SPA_HTR,aqdata->aqbuttons[i].name) == 0) {
      length += sprintf(msg+length, ", Spa Heat LED=%d ",aqdata->aqbuttons[i].led->state);
    }
  }

  length += sprintf(msg+length, ", Pool SP=%d, Spa SP=%d",aqdata->pool_htr_set_point, aqdata->spa_htr_set_point);
  length += sprintf(msg+length, ", Pool temp=%d, Spa temp=%d",aqdata->pool_temp, aqdata->spa_temp);

  LOG(DJAN_LOG, LOG_INFO, "%s\n", msg);

  return false;
}


bool processPacketToJandyChemFeeder(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata)
{
  /*
  char msg[1024];
  int length = 0;

  length += sprintf(msg+length, "Last panel info ");
  length += sprintf(msg+length, ", pH=%f, ORP=%d",aqdata->ph, aqdata->orp);
  LOG(DJAN_LOG, LOG_INFO, "%s\n", msg);
*/
  LOG(DJAN_LOG, LOG_INFO, "Last panel info : pH=%f, ORP=%d",aqdata->ph, aqdata->orp);

  if (!shouldPrintJAndyDebugPacket()) {
    // if we are not going to print the packet as debug, print it here
    logPacket(DJAN_LOG, LOG_INFO, packet_buffer, packet_length, true);
  }
  
  return false;
}

bool processPacketFromJandyChemFeeder(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata, const unsigned char previous_packet_to){
/*
  char msg[1024];
  int length = 0;

  length += sprintf(msg+length, "Last panel info ");
  length += sprintf(msg+length, ", pH=%f, ORP=%d",aqdata->ph, aqdata->orp);
  LOG(DJAN_LOG, LOG_INFO, "%s\n", msg);
*/
  LOG(DJAN_LOG, LOG_INFO, "Last panel info : pH=%f, ORP=%d",aqdata->ph, aqdata->orp);
  
  if (!shouldPrintJAndyDebugPacket()) {
    // if we are not going to print the packet as debug, print it here
    logPacket(DJAN_LOG, LOG_INFO, packet_buffer, packet_length, true);
  }
  
  /*
  I think the below may be accurate
  ph_setpoint  = float(raw_data[8]) / 10
  acl_setpoint = raw_data[9] * 10
  ph_current   = float(raw_data[10]) / 10
  acl_current  = raw_data[11] * 10
  */
  
  /*
  from https://community.home-assistant.io/t/reading-orp-ph-from-old-watermatic-jandy-chemlink1500/933085
  10 02                                  <-- uartex header
  00 21                                  <-- chemistry frame marker
  02 41                                  <-- tag02: ORP=0x41=65 → 650 mV
  03 4B                                  <-- tag03: pH=0x4B=75 → 7.5
  08 00                                  <-- tag08: pH feeder active (0x00)
  18 01                                  <-- tag18: ORP feed ON
  60                                     <-- checksum (sum&0x7F)
  10 03                                  <-- uartex footer

  ORP is tag 0x02 (0x41=65)          (value × 10, 200–1200 mV accepted).
  pH is found at tag 0x03 (0x4B=75). (value ÷ 10, only 5.0–10.0 accepted).

  0x41 (65) ×10 = 650 mV (ORP)
  0x4B (75) ÷10 = 7.5 pH
  tag08=0x00 → pH feeder running
  tag18=0x01 → ORP feed ON
  
  0x10|0x02|0x00|0x21|0x02|0x41|0x03|0x4B|0x08|0x00|0x18|0x01|0x60|0x10|0x03

  */

  return false;
}




// ---- pH ----
float ph_from_counts(int counts, float temp_c) {
    
    //    Convert ADC counts + water temperature to pH.
    //    counts  : ADC counts (0..4095)
    //    temp_c  : water temperature in °C
    //    returns : pH value
    

    const int n_bits = 12;
    const float v_ref = 3.3f;       // ADC reference voltage
    const float v_mid = 1.650f;     // mid-rail voltage representing pH 7
    const float gain_pH = 12.745f;  // amplifier gain

    // Step 1: counts -> voltage
    float v = ((float)counts / (float)((1 << n_bits) - 1)) * v_ref;

    // Step 2: electrode mV after gain removal
    float v_elec_mV = (v - v_mid) * 1000.0f / gain_pH;

    // Step 3: Nernst slope at water temperature
    float slope = 59.16f * (temp_c + 273.15f) / 298.15f; // mV/pH

    // Step 4: pH
    return 7.0f + (v_elec_mV / slope);
}

// ---- ORP ----
float orp_from_counts(int counts) {
    /*
        Convert ADC counts to ORP (mV)
        counts : ADC counts (0..4095)
        returns: ORP in millivolts
    */
    return -1909.25f + 0.93294f * (float)counts;
}

// ---- Example usage ----
/*
int main(void) {
    int counts_ph  = 2341;  // example from Group 1
    int counts_orp = 2916;  // example from Group 2
    float temp_c   = 38.0f; // water temperature in °C

    float ph_value  = ph_from_counts(counts_ph, temp_c);
    float orp_value = orp_from_counts(counts_orp);

    printf("pH  = %.2f\n", ph_value);
    printf("ORP = %.0f mV\n", orp_value);

    return 0;
}
*/

bool processPacketToJandyChemAnalyzer(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata)
{
  
  //LOG(DJAN_LOG, LOG_INFO, "Last panel info pH=%f, ORP=%d\n",aqdata->ph, aqdata->orp, );

  return false;
}

bool processPacketFromJandyChemAnalyzer(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata, const unsigned char previous_packet_to){
  
  int watertemp = 0;

  if (isCOMBO_PANEL && aqdata->aqbuttons[SPA_INDEX].led->state == ON) {
    watertemp = aqdata->spa_temp;
    LOG(DJAN_LOG, LOG_INFO, "Last panel info pH=%f, ORP=%d, Spa water temp=%d (Spamode)\n",aqdata->ph, aqdata->orp, aqdata->spa_temp  );
  } else {
    watertemp = aqdata->pool_temp;
    LOG(DJAN_LOG, LOG_INFO, "Last panel info pH=%f, ORP=%d, Pool water temp=%d\n",aqdata->ph, aqdata->orp, aqdata->pool_temp  );
  }


  if (watertemp <= 0) {
    if (previous_packet_to == 0x84){
      if (packet_buffer[3] == 0x28 && packet_buffer[5] == 0x01) {
        float ph = ph_from_counts( ((packet_buffer[6] * 256) + packet_buffer[7]),
                                    aqdata->temp_units==FAHRENHEIT?roundf(degFtoC(watertemp)):watertemp);
        LOG(DJAN_LOG, LOG_INFO, "Guess at caculating pH=%f\n", ph);                            
      }
    } else if (previous_packet_to == 0x84){
      if (packet_buffer[3] == 0x28 && packet_buffer[5] == 0x02) {
        float orp = orp_from_counts( ((packet_buffer[6] * 256) + packet_buffer[7]));
        LOG(DJAN_LOG, LOG_INFO, "Guess at caculating ORP=%f\n", orp);
      }
    }
  } else {
    LOG(DJAN_LOG, LOG_INFO, "ORP & pH not caculated as watertemp %d is out of range\n", watertemp);
  }

  return false;
}



bool processPacketToHeatPump(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata)
{
/* Byted 3 and 4
  0x0c|0x01 = Heat Pump Enabled
  0x0c|0x29 = Chiller on
  0x0c|0x00 = Off
  0x0c|0x09 =  inknown at present
  0x0c|0x0a =  unknown at present
*/
/*
  0x0c|0x00 = Request off
  0x0c|0x09 = Request Heat
  0x0c|0x29 = Request Cool
*/
/*
0x0c|0x00|0x00|0x00|0x00|	CMD HP Disabled
0x0c|0x0a|0x00|0x00|0x00|	CMD HP Heat SPA
0x0c|0x01|0x00|0x00|0x00|	CMD HP Enabled Pool
0x0c|0x02|0x00|0x00|0x00|	CMD HP Enabled SPA
0x0c|0x09|0x00|0x00|0x00|	CMD HP Heat Pool
*/
  if (packet_buffer[3] == 0x0c ) {
    switch(packet_buffer[4]) {
      case 0x00: // Heat Pump is off
        LOG(DJAN_LOG, LOG_DEBUG, "Heat Pump 0x%02hhx request to Off - status 0x%02hhx\n",packet_buffer[PKT_DEST],packet_buffer[4] );
        updateHeatPumpLed(HP_UNKNOWN, OFF, aqdata, HP_FROM_PANEL);
      break;
      case 0x09: // Heat Pool
      case 0x0a: // Heat Spa
        LOG(DJAN_LOG, LOG_DEBUG, "Heat Pump 0x%02hhx request to Heat - status 0x%02hhx\n",packet_buffer[PKT_DEST],packet_buffer[4] );
        updateHeatPumpLed(HP_HEAT, ON, aqdata, HP_FROM_PANEL);
      break;
      case 0x01: // Enabled Pool
      case 0x02: // Enabled SPA
        LOG(DJAN_LOG, LOG_DEBUG, "Heat Pump 0x%02hhx Enabled - status 0x%02hhx\n",packet_buffer[PKT_DEST],packet_buffer[4] );
        updateHeatPumpLed(HP_UNKNOWN, ENABLE, aqdata, HP_FROM_PANEL);
      break;
      case 0x29: // Cool
        LOG(DJAN_LOG, LOG_DEBUG, "Heat Pump 0x%02hhx request to Cool - status 0x%02hhx\n",packet_buffer[PKT_DEST],packet_buffer[4] );
        updateHeatPumpLed(HP_COOL, ENABLE, aqdata, HP_FROM_PANEL);
      break;
      default:
        LOG(DJAN_LOG, LOG_INFO, "Heat Pump 0x%02hhx request to (unknown status) 0x%02hhx\n",packet_buffer[PKT_DEST], packet_buffer[4]);
        //if (aqdata->chiller_button != NULL && aqdata->chiller_button->led->state == OFF)
        //  updateHeatPumpLed(HP_UNKNOWN, ENABLE, aqdata, HP_FROM_PANEL); // Guess at enabled. ()
      break;
    }
  } else {
    LOG(DJAN_LOG, LOG_INFO, "Heat Pump 0x%02hhx request unknown 0x%02hhx 0x%02hhx\n",packet_buffer[PKT_DEST], packet_buffer[3] , packet_buffer[4]);
  }


  return false;
}
bool processPacketFromHeatPump(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata, const unsigned char previous_packet_to)
{
/*
  HEX: 0x10|0x02|0x00|0x0d|0x40|0x00|0x00|0x5f|0x10|0x03|
  HEX: 0x10|0x02|0x00|0x0d|0x48|0x00|0x00|0x67|0x10|0x03|
  HEX: 0x10|0x02|0x00|0x0d|0x68|0x00|0x00|0x87|0x10|0x03|

  // Reply is some status 0x40,0x48,0x68
*/
  // 0x40 = OFF
  // 0x48 = HEATING.
  // 0x68 = COOL.
  
  if (packet_buffer[3] == 0x0d ) {
    if (packet_buffer[4] == 0x40) {
      //updateHeatPumpLed(HP_HEAT, OFF, aqdata, HP_TO_PANEL);
      updateHeatPumpLed(HP_HEAT, ENABLE, aqdata, HP_TO_PANEL);
    } else if (packet_buffer[4] == 0x48) {
      updateHeatPumpLed(HP_HEAT, ON, aqdata, HP_TO_PANEL);
    } else if (packet_buffer[4] == 0x68) {
      updateHeatPumpLed(HP_COOL, ON, aqdata, HP_TO_PANEL);
    } else {
      //LOG(DJAN_LOG, LOG_INFO, "Heat Pump 0x%02hhx ");
      LOG(DJAN_LOG, LOG_INFO, "Heat Pump 0x%02hhx returned unknown state 0x%02hhx\n",packet_buffer[PKT_DEST], packet_buffer[4]);
    }
  } else {
    LOG(DJAN_LOG, LOG_INFO, "Heat Pump 0x%02hhx returned unknown information 0x%02hhx 0x%02hhx\n",packet_buffer[PKT_DEST], packet_buffer[3], packet_buffer[4]);
  }

  return false;
}


void processHeatPumpDisplayMessage(char *msg, struct aqualinkdata *aqdata) {
  // Could get messages like below.
  // 'Heat Pump ENA'
  // '        Heat Pump ENA  '
  // 'Heat Pump Enabled'
  // Or chiller.
  heatpumpstate hpstate = HP_HEAT;

  // are we heat pump or chiller
  if (stristr(msg,"Chiller") != NULL) {
    // NSF Should check alt_mode is Chiller and not Heat Pump
    ((altlabel_detail *)aqdata->chiller_button->special_mask_ptr)->in_alt_mode = true;
    hpstate = HP_COOL;
  }
  if (stristr(msg," ENA") != NULL) {
    updateHeatPumpLed(hpstate, ENABLE, aqdata, HP_DISPLAY);
  } else if (stristr(msg," OFF") != NULL) {
    updateHeatPumpLed(hpstate, OFF, aqdata, HP_DISPLAY);
  } else if (stristr(msg," ON") != NULL) {
    updateHeatPumpLed(hpstate, ON, aqdata, HP_DISPLAY);
  }

  LOG(AQUA_LOG,LOG_DEBUG, "Set %s to %s from message '%s'",
    ((altlabel_detail *)aqdata->chiller_button->special_mask_ptr)->in_alt_mode?((altlabel_detail *)aqdata->chiller_button->special_mask_ptr)->altlabel:aqdata->chiller_button->label,
      LED2text(aqdata->chiller_button->led->state), msg);
}

//void updateHeatPumpLed(bool chiller, aqledstate state, struct aqualinkdata *aqdata) {
void updateHeatPumpLed(heatpumpstate state, aqledstate ledstate, struct aqualinkdata *aqdata, heatpumpmsgfrom from)
{
  if (aqdata->chiller_button == NULL)
    return;
  
  // ledstate Enabled is valied from Display and FromPanel HP_FROM_PANEL, HP_DISPLAY (NOT valid from HP ie HP_TO_PANEL)
  // ledstate on off is only valid from HP or DISPLAY HP_TO_PANEL or HP_TO_PANEL (NOT FROM HP_FROM_PANEL)

  if ( (ledstate == ENABLE && (from == HP_DISPLAY || from == HP_FROM_PANEL)) ||
       ( (ledstate == ON || ledstate == OFF) && (from == HP_DISPLAY || from == HP_TO_PANEL) )) {
    SET_IF_CHANGED(aqdata->chiller_button->led->state, ledstate, aqdata->is_dirty);
  }

  if (state == HP_COOL) {
    ((altlabel_detail *)aqdata->chiller_button->special_mask_ptr)->in_alt_mode = true;
  } else if (state == HP_HEAT) {
    ((altlabel_detail *)aqdata->chiller_button->special_mask_ptr)->in_alt_mode = false;
  }
}


bool processPacketToJandyLight(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata)
{
  if (packet_buffer[3] == 0x4b) {
    LOG(DJAN_LOG, LOG_INFO, "Request to set Jandy Light brightness to %d\n", packet_buffer[6]);
  } else if (packet_buffer[3] == 0x3a) {
    LOG(DJAN_LOG, LOG_INFO, "Request to set Jandy Light RGB to %d:%d:%d\n", packet_buffer[6],packet_buffer[7],packet_buffer[8]);
  }

  if (!shouldPrintJAndyDebugPacket()) {
    // if we are not going to print the packet as debug, print it here
    logPacket(DJAN_LOG, LOG_INFO, packet_buffer, packet_length, true);
  }
  
  return true;
}

bool processPacketFromJandyLight(unsigned char *packet_buffer, int packet_length, struct aqualinkdata *aqdata, const unsigned char previous_packet_to)
{
  if (packet_buffer[3] == 0x31 ) {
    // This has most of the info.
    LOG(DJAN_LOG, LOG_INFO, "Light brightness=%d\n", packet_buffer[38]);
  }

  if (!shouldPrintJAndyDebugPacket()) {
    // if we are not going to print the packet as debug, print it here
    logPacket(DJAN_LOG, LOG_INFO, packet_buffer, packet_length, true);
  }
  
  return true;
}


/*

// JXi Heater

// Normal ping and return
5th bit 0x00 no pump on (nothing)
        0x10 seems to be JXi came online. nothing more
        0x11 (pool mode)
        0x12 (spa mode)
        0x19 heat pool
        0x1a heat spa
6th bit 0x55 = 85 deg. (current pool setpoint)
7th bit 0x66 = 102 deg. (current spa setpoint)
8th bit 0x4f = current water temp 79 (0xFF is off / 255)

Jandy     To 0x68 of type   Unknown '0x0c' | HEX: 0x10|0x02|0x68|0x0c|0x11|0x55|0x66|0x4f|0xa1|0x10|0x03|
Jandy   From 0x68 of type   Unknown '0x0d' | HEX: 0x10|0x02|0x00|0x0d|0x00|0x00|0x00|0x1f|0x10|0x03|

Request to turn on 85  
5th bit 0x19 looks like turn on
6th bit 0x55 = 85 deg.
7th bit 0x4f = current temp 79
Jandy     To 0x68 of type   Unknown '0x0c' | HEX: 0x10|0x02|0x68|0x0c|0x19|0x55|0x66|0x4f|0xa9|0x10|0x03|
Jandy   From 0x68 of type   Unknown '0x0d' | HEX: 0x10|0x02|0x00|0x0d|0x08|0x00|0x00|0x27|0x10|0x03|

Request to turn on 90
5th bit 0x19 looks like turn on
6th bit 0x5a = 90 deg.
Jandy     To 0x68 of type   Unknown '0x0c' | HEX: 0x10|0x02|0x68|0x0c|0x19|0x5a|0x66|0x4f|0xae|0x10|0x03|
Jandy   From 0x68 of type   Unknown '0x0d' | HEX: 0x10|0x02|0x00|0x0d|0x08|0x00|0x00|0x27|0x10|0x03|

Request to turn off (standard ping)  // return had hi limit error in it
Jandy     To 0x68 of type   Unknown '0x0c' | HEX: 0x10|0x02|0x68|0x0c|0x11|0x55|0x66|0x4f|0xa1|0x10|0x03|
Jandy   From 0x68 of type   Unknown '0x0d' | HEX: 0x10|0x02|0x00|0x0d|0x00|0x00|0x10|0x2f|0x10|0x03|

Returns

5th bit is type 0x00 nothing (or enabeled) - 0x08 looks like heat 
Hi limit error return
7th bit 0x10 looks like the error
Jandy     To 0x68 of type   Unknown '0x0c' | HEX: 0x10|0x02|0x68|0x0c|0x19|0x5a|0x66|0x4f|0xae|0x10|0x03|
Jandy   From 0x68 of type   Unknown '0x0d' | HEX: 0x10|0x02|0x00|0x0d|0x08|0x00|0x10|0x37|0x10|0x03|

Errors are ->
check flow
Fault high limit -> 0x10
Fault High Flu temp
Fault Check Igntion Control
Fault Short H20 sensor (or Fault open water sensor) -> 0x02
Pump fault
AUX Monitor -> 0x08
*/


/*

Heat Pump Chiller.  Messages are in this thread.
https://github.com/sfeakes/AqualinkD/discussions/391#discussioncomment-12431509

LXi heater ping | HEX: 0x10|0x02|0x70|0x0c|0x09|0x00|0x00|0x00|0x97|0x10|0x03|
LXi status      | HEX: 0x10|0x02|0x00|0x0d|0x48|0x00|0x00|0x67|0x10|0x03|

LXi heater ping | HEX: 0x10|0x02|0x70|0x0c|0x00|0x00|0x00|0x00|0x8e|0x10|0x03|   byte 4 0x00 is OFF.
LXi status      | HEX: 0x10|0x02|0x00|0x0d|0x40|0x00|0x00|0x5f|0x10|0x03|        0x40 (maybe off, but odd message for off)

LXi heater ping | HEX: 0x10|0x02|0x70|0x0c|0x29|0x00|0x00|0x00|0xb7|0x10|0x03|.   byte 4 0x29 This is some on / enable
LXi status      | HEX: 0x10|0x02|0x00|0x0d|0x68|0x00|0x00|0x87|0x10|0x03|         0x68 probably is chiller ON

Below is when heatpump chiller is enabled but NOT on (heat or cool)
JandyDvce: To   HPump: Read  Jandy   packet To 0x70 of type  LXi heater ping | HEX: 0x10|0x02|0x70|0x0c|0x01|0x00|0x00|0x00|0x8f|0x10|0x03|
JandyDvce: From HPump: Read  Jandy   packet To 0x00 of type       LXi status | HEX: 0x10|0x02|0x00|0x0d|0x40|0x00|0x00|0x5f|0x10|0x03|

0x0c|0x01 = Enabled
0x0c|0x29 = Chiller on
0x0c|0x00 = Off

Better Info
Heat Pump Enabled
JandyDvce: To   HPump: Read  Jandy   packet To 0x70 of type  LXi heater ping | HEX: 0x10|0x02|0x70|0x0c|0x01|0x00|0x00|0x00|0x8f|0x10|0x03|
JandyDvce: From HPump: Read  Jandy   packet To 0x00 of type       LXi status | HEX: 0x10|0x02|0x00|0x0d|0x40|0x00|0x00|0x5f|0x10|0x03|

Heat Pump (Chiller ON)
JandyDvce: To   HPump: Read  Jandy   packet To 0x70 of type  LXi heater ping | HEX: 0x10|0x02|0x70|0x0c|0x29|0x00|0x00|0x00|0xb7|0x10|0x03|
JandyDvce: From HPump: Read  Jandy   packet To 0x00 of type       LXi status | HEX: 0x10|0x02|0x00|0x0d|0x68|0x00|0x00|0x87|0x10|0x03|

Heat Pump Enabled
JandyDvce: To   HPump: Read  Jandy   packet To 0x70 of type  LXi heater ping | HEX: 0x10|0x02|0x70|0x0c|0x01|0x00|0x00|0x00|0x8f|0x10|0x03|
JandyDvce: From HPump: Read  Jandy   packet To 0x00 of type       LXi status | HEX: 0x10|0x02|0x00|0x0d|0x40|0x00|0x00|0x5f|0x10|0x03|

Heat Pump Off
JandyDvce: To   HPump: Read  Jandy   packet To 0x70 of type  LXi heater ping | HEX: 0x10|0x02|0x70|0x0c|0x00|0x00|0x00|0x00|0x8e|0x10|0x03|
JandyDvce: From HPump: Read  Jandy   packet To 0x00 of type       LXi status | HEX: 0x10|0x02|0x00|0x0d|0x40|0x00|0x00|0x5f|0x10|0x03|

*/



/*
RS485 Color Lights.

Turned lights on (was already set to "America the Beautiful")     <-  USA?? index 12
Switch color to "Alpine White".                                   <-  index 1
Turned brightness down to 50%
Turned brightness down to 25%
Turned off


packet To 0xf0 of type   Unknown '0x32' | HEX: 0x10|0x02|0xf0|0x32|0x00|0x34|0x10|0x03|
packet To 0x00 of type   Unknown '0x33' | HEX: 0x10|0x02|0x00|0x33|0x2d|0x00|0x72|0x10|0x03|
--
packet To 0xf0 of type   Unknown '0x32' | HEX: 0x10|0x02|0xf0|0x32|0x00|0x34|0x10|0x03|
packet To 0x00 of type   Unknown '0x33' | HEX: 0x10|0x02|0x00|0x33|0x2d|0x10|0x82|0x10|0x03|
--
******. Below reply byte 11 = 0x23 (color mode)????
packet To 0xf0 of type         iAq Poll | HEX: 0x10|0x02|0xf0|0x30|0x00|0x32|0x10|0x03|
packet To 0x00 of type iAq receive read | HEX: 0x10|0x02|0x00|0x31|0x2d|0x06|0x02|0x22|0x02|0x21|0x01|0x23|0x02|0x21|0x01|0x23|0x01|0x24|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0x64|0x64|0x64|0x64|0x00|0x00|0x00|0x81|0x0c|0x0f|0x03|0x72|0x10|0x03|
--
packet To 0xf0 of type   Unknown '0x32' | HEX: 0x10|0x02|0xf0|0x32|0x00|0x34|0x10|0x03|
packet To 0x00 of type   Unknown '0x33' | HEX: 0x10|0x02|0x00|0x33|0x2d|0x00|0x72|0x10|0x03|
--
packet To 0xf0 of type   Unknown '0x32' | HEX: 0x10|0x02|0xf0|0x32|0x00|0x34|0x10|0x03|
packet To 0x00 of type   Unknown '0x33' | HEX: 0x10|0x02|0x00|0x33|0x2d|0x10|0x82|0x10|0x03|
--
***** Below reply packed 38 is 0x64 (brightness 100%)
***** Below reply packet 11 = 0x24 (color mode)?????
packet To 0xf0 of type         iAq Poll | HEX: 0x10|0x02|0xf0|0x30|0x00|0x32|0x10|0x03|
packet To 0x00 of type iAq receive read | HEX: 0x10|0x02|0x00|0x31|0x2d|0x06|0x02|0x22|0x02|0x21|0x01|0x24|0x02|0x21|0x01|0x24|0x01|0x24|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0x64|0x64|0x64|0x64|0x00|0x00|0x00|0x81|0x0c|0x0f|0x03|0x74|0x10|0x03|
--
packet To 0xf0 of type   Unknown '0x32' | HEX: 0x10|0x02|0xf0|0x32|0x00|0x34|0x10|0x03|
packet To 0x00 of type   Unknown '0x33' | HEX: 0x10|0x02|0x00|0x33|0x2d|0x00|0x72|0x10|0x03|
--
packet To 0xf0 of type   Unknown '0x32' | HEX: 0x10|0x02|0xf0|0x32|0x00|0x34|0x10|0x03|
packet To 0x00 of type   Unknown '0x33' | HEX: 0x10|0x02|0x00|0x33|0x2d|0x00|0x72|0x10|0x03|
--
packet To 0xf0 of type   Unknown '0x32' | HEX: 0x10|0x02|0xf0|0x32|0x00|0x34|0x10|0x03|
packet To 0x00 of type   Unknown '0x33' | HEX: 0x10|0x02|0x00|0x33|0x2d|0x00|0x72|0x10|0x03|
--
********** Below Change to 50% ***********
packet To 0xf0 of type   Unknown '0x4b' | HEX: 0x10|0x02|0xf0|0x4b|0x00|0x01|0x32|0x00|0x00|0x00|0x00|0x80|0x10|0x03|
packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x4b|0x00|0x5e|0x10|0x03|
--
packet To 0xf0 of type   Unknown '0x32' | HEX: 0x10|0x02|0xf0|0x32|0x00|0x34|0x10|0x03|
packet To 0x00 of type   Unknown '0x33' | HEX: 0x10|0x02|0x00|0x33|0x2d|0x50|0xc2|0x10|0x03|
--
***** Below reply packed 38 is 0x32 (brightness 50%)
packet To 0xf0 of type         iAq Poll | HEX: 0x10|0x02|0xf0|0x30|0x00|0x32|0x10|0x03|
packet To 0x00 of type iAq receive read | HEX: 0x10|0x02|0x00|0x31|0x2d|0x06|0x02|0x22|0x02|0x21|0x01|0x23|0x02|0x21|0x01|0x23|0x01|0x24|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0x32|0x64|0x64|0x64|0x00|0x00|0x00|0x81|0x0c|0x0f|0x03|0x40|0x10|0x03|
--
packet To 0xf0 of type   Unknown '0x32' | HEX: 0x10|0x02|0xf0|0x32|0x00|0x34|0x10|0x03|
packet To 0x00 of type   Unknown '0x33' | HEX: 0x10|0x02|0x00|0x33|0x2d|0x02|0x74|0x10|0x03|
--
packet To 0xf0 of type   Unknown '0x32' | HEX: 0x10|0x02|0xf0|0x32|0x00|0x34|0x10|0x03|
packet To 0x00 of type   Unknown '0x33' | HEX: 0x10|0x02|0x00|0x33|0x2d|0x00|0x72|0x10|0x03|
--
***** Below reply packed 38 is 0x19 (brightness 25%)
*********** Below Change to 25% ************
packet To 0xf0 of type   Unknown '0x4b' | HEX: 0x10|0x02|0xf0|0x4b|0x00|0x01|0x19|0x00|0x00|0x00|0x00|0x67|0x10|0x03|
packet To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x4b|0x00|0x5e|0x10|0x03|
--
packet To 0xf0 of type   Unknown '0x32' | HEX: 0x10|0x02|0xf0|0x32|0x00|0x34|0x10|0x03|
packet To 0x00 of type   Unknown '0x33' | HEX: 0x10|0x02|0x00|0x33|0x2d|0x50|0xc2|0x10|0x03|
--
packet To 0xf0 of type         iAq Poll | HEX: 0x10|0x02|0xf0|0x30|0x00|0x32|0x10|0x03|
packet To 0x00 of type iAq receive read | HEX: 0x10|0x02|0x00|0x31|0x2d|0x06|0x02|0x22|0x02|0x21|0x01|0x23|0x02|0x21|0x01|0x23|0x01|0x23|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0x19|0x64|0x64|0x64|0x00|0x00|0x00|0x81|0x0c|0x0f|0x03|0x26|0x10|0x03|
--
packet To 0xf0 of type   Unknown '0x32' | HEX: 0x10|0x02|0xf0|0x32|0x00|0x34|0x10|0x03|
packet To 0x00 of type   Unknown '0x33' | HEX: 0x10|0x02|0x00|0x33|0x2d|0x00|0x72|0x10|0x03|
--
packet To 0xf0 of type   Unknown '0x32' | HEX: 0x10|0x02|0xf0|0x32|0x00|0x34|0x10|0x03|
packet To 0x00 of type   Unknown '0x33' | HEX: 0x10|0x02|0x00|0x33|0x2d|0x00|0x72|0x10|0x03|
--
packet To 0xf0 of type   Unknown '0x32' | HEX: 0x10|0x02|0xf0|0x32|0x00|0x34|0x10|0x03|
packet To 0x00 of type   Unknown '0x33' | HEX: 0x10|0x02|0x00|0x33|0x2d|0x40|0xb2|0x10|0x03|
--
packet To 0xf0 of type         iAq Poll | HEX: 0x10|0x02|0xf0|0x30|0x00|0x32|0x10|0x03|
packet To 0x00 of type iAq receive read | HEX: 0x10|0x02|0x00|0x31|0x2d|0x06|0x02|0x22|0x02|0x21|0x01|0x23|0x02|0x21|0x01|0x23|0x01|0x23|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0x19|0x64|0x64|0x64|0x00|0x00|0x00|0x01|0x0c|0x0f|0x03|0xa6|0x10|0x03|
--
packet To 0xf0 of type   Unknown '0x32' | HEX: 0x10|0x02|0xf0|0x32|0x00|0x34|0x10|0x03|
packet To 0x00 of type   Unknown '0x33' | HEX: 0x10|0x02|0x00|0x33|0x2d|0x10|0x82|0x10|0x03|
--
packet To 0xf0 of type         iAq Poll | HEX: 0x10|0x02|0xf0|0x30|0x00|0x32|0x10|0x03|
packet To 0x00 of type iAq receive read | HEX: 0x10|0x02|0x00|0x31|0x2d|0x06|0x02|0x22|0x02|0x21|0x01|0x22|0x02|0x21|0x01|0x23|0x01|0x23|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0xff|0x00|0x19|0x64|0x64|0x64|0x00|0x00|0x00|0x01|0x0c|0x0f|0x03|0xa5|0x10|0x03|





*/



/*

**************

TruSense

Below is the repeat look (looks like 2 ID's) = is one for ORP and other pH ?????

First 3 to 0x84, are requests 0x00,0x01,0x03. So ignore that in reply and everything else you get data at
Last 2 bytes (below)
0x02|0x14
0x09|0x21
0x00|0x00

Similar for 0x86 at you get
0x0b|0x6a


As two independent bytes	9 and 33
Unsigned 16-bit, big-endian (0x09 is high byte)	
0x09 × 256 + 0x21 = 2304 + 33 = 2337
0x09×256+0x21= 2304+33 =2337

Unsigned 16-bit, little-endian (0x21 is high byte)	
0x21 × 256 + 0x09 = 8448 + 9 = 8457
0x21×256+0x09= 8448+9 = 8457



To 0x84 of type            Probe | HEX: 0x10|0x02|0x84|0x00|0x96|0x10|0x03|
To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x00|0x00|0x13|0x10|0x03|

To 0x84 of type   Unknown '0x20' | HEX: 0x10|0x02|0x84|0x20|0x00|0xb6|0x10|0x03|
To 0x00 of type      iAq PageEnd | HEX: 0x10|0x02|0x00|0x28|0x20|0x00|0x02|0x14|0x70|0x10|0x03|

To 0x84 of type   Unknown '0x20' | HEX: 0x10|0x02|0x84|0x20|0x01|0xb7|0x10|0x03|
To 0x00 of type      iAq PageEnd | HEX: 0x10|0x02|0x00|0x28|0x20|0x01|0x09|0x21|0x85|0x10|0x03|

To 0x84 of type   Unknown '0x20' | HEX: 0x10|0x02|0x84|0x20|0x03|0xb9|0x10|0x03|
To 0x00 of type      iAq PageEnd | HEX: 0x10|0x02|0x00|0x28|0x20|0x03|0x00|0x00|0x5d|0x10|0x03|

To 0x86 of type            Probe | HEX: 0x10|0x02|0x86|0x00|0x98|0x10|0x03|
To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x00|0x00|0x13|0x10|0x03|

To 0x86 of type   Unknown '0x20' | HEX: 0x10|0x02|0x86|0x20|0x02|0xba|0x10|0x03|
To 0x00 of type      iAq PageEnd | HEX: 0x10|0x02|0x00|0x28|0x20|0x02|0x0b|0x6a|0xd1|0x10|0x03|



# list of unique 0x84 returns all represent ORP:810 or pH:7.3 (water temp 100 and 102)
To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x00|0x00|0x13|0x10|0x03|
To 0x00 of type      iAq PageEnd | HEX: 0x10|0x02|0x00|0x28|0x20|0x00|0x02|0x14|0x70|0x10|0x03|

To 0x00 of type      iAq PageEnd | HEX: 0x10|0x02|0x00|0x28|0x20|0x01|0x09|0x21|0x85|0x10|0x03|
To 0x00 of type      iAq PageEnd | HEX: 0x10|0x02|0x00|0x28|0x20|0x01|0x09|0x22|0x86|0x10|0x03|
To 0x00 of type      iAq PageEnd | HEX: 0x10|0x02|0x00|0x28|0x20|0x01|0x09|0x27|0x8b|0x10|0x03|
To 0x00 of type      iAq PageEnd | HEX: 0x10|0x02|0x00|0x28|0x20|0x01|0x09|0x28|0x8c|0x10|0x03|

To 0x00 of type      iAq PageEnd | HEX: 0x10|0x02|0x00|0x28|0x20|0x03|0x00|0x00|0x5d|0x10|0x03|

# list of unique 0x86 returns all represent ORP:810 or pH:7.3 (water temp 100 and 102)
To 0x00 of type              Ack | HEX: 0x10|0x02|0x00|0x01|0x00|0x00|0x13|0x10|0x03|
To 0x00 of type      iAq PageEnd | HEX: 0x10|0x02|0x00|0x28|0x20|0x02|0x0b|0x54|0xbb|0x10|0x03|
To 0x00 of type      iAq PageEnd | HEX: 0x10|0x02|0x00|0x28|0x20|0x02|0x0b|0x55|0xbc|0x10|0x03|
To 0x00 of type      iAq PageEnd | HEX: 0x10|0x02|0x00|0x28|0x20|0x02|0x0b|0x57|0xbe|0x10|0x03|
To 0x00 of type      iAq PageEnd | HEX: 0x10|0x02|0x00|0x28|0x20|0x02|0x0b|0x59|0xc0|0x10|0x03|
To 0x00 of type      iAq PageEnd | HEX: 0x10|0x02|0x00|0x28|0x20|0x02|0x0b|0x69|0xd0|0x10|0x03|
To 0x00 of type      iAq PageEnd | HEX: 0x10|0x02|0x00|0x28|0x20|0x02|0x0b|0x6a|0xd1|0x10|0x03|
To 0x00 of type      iAq PageEnd | HEX: 0x10|0x02|0x00|0x28|0x20|0x02|0x0b|0x6b|0xd2|0x10|0x03|
To 0x00 of type      iAq PageEnd | HEX: 0x10|0x02|0x00|0x28|0x20|0x02|0x0b|0x6c|0xd3|0x10|0x03|
To 0x00 of type      iAq PageEnd | HEX: 0x10|0x02|0x00|0x28|0x20|0x02|0x0b|0x6d|0xd4|0x10|0x03|




*/