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
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <sys/time.h>
#include <syslog.h>


#include "mongoose.h"

#include "aqualink.h"
#include "config.h"
#include "aq_programmer.h"
#include "utils.h"
#include "net_services.h"
#include "json_messages.h"
#include "domoticz.h"
#include "aq_mqtt.h"


static struct aqconfig *_aqualink_config;
static struct aqualinkdata *_aqualink_data;
static char *_web_root;

static int _mqtt_exit_flag = false;
/*
static const char *s_address = "trident:1883";
static const char *s_user_name = NULL;
static const char *s_password = NULL;
static const char *s_topic = "domoticz/out";
*/
//static struct mg_mqtt_topic_expression s_topic_expr = {NULL, 0};



#ifndef MG_DISABLE_MQTT
void start_mqtt(struct mg_mgr *mgr);
static struct aqualinkdata _last_mqtt_aqualinkdata;
void mqtt_broadcast_aqualinkstate(struct mg_connection *nc);
#endif


static sig_atomic_t s_signal_received = 0;
//static const char *s_http_port = "8080";
static struct mg_serve_http_opts s_http_server_opts;

static void signal_handler(int sig_num) {
  signal(sig_num, signal_handler);  // Reinstantiate signal handler
  s_signal_received = sig_num;
}


static int is_websocket(const struct mg_connection *nc) {
  return nc->flags & MG_F_IS_WEBSOCKET;
}
static int is_mqtt(const struct mg_connection *nc) {
  return nc->flags & MG_F_USER_1;
}
static void set_mqtt(struct mg_connection *nc) {
  nc->flags |= MG_F_USER_1; 
}

static void ws_send(struct mg_connection *nc, char *msg)
{
  int size = strlen(msg);
  
  mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, msg, size);
  
  //logMessage (LOG_DEBUG, "WS: Sent %d characters '%s'\n",size, msg);
}

void broadcast_aqualinkstate_error(struct mg_connection *nc, char *msg) 
{
  struct mg_connection *c;
  char data[JSON_STATUS_SIZE];
  build_aqualink_error_status_JSON(data, JSON_STATUS_SIZE, msg);

  for (c = mg_next(nc->mgr, NULL); c != NULL; c = mg_next(nc->mgr, c)) {
    if (is_websocket(c))
      ws_send(c, data);
  }
  // Maybe enhacment in future to sent error messages to MQTT
}

void broadcast_aqualinkstate(struct mg_connection *nc) 
{
  static int mqtt_count=0;
  struct mg_connection *c;
  char data[JSON_STATUS_SIZE];
  build_aqualink_status_JSON(_aqualink_data, data, JSON_STATUS_SIZE);
  
#ifndef MG_DISABLE_MQTT  
  if (_mqtt_exit_flag == true) {
    mqtt_count++;
    if (mqtt_count >= 10) {
      start_mqtt(nc->mgr);
      mqtt_count = 0;
    }
  }
#endif

  for (c = mg_next(nc->mgr, NULL); c != NULL; c = mg_next(nc->mgr, c)) {
    if (is_websocket(c))
      ws_send(c, data);
#ifndef MG_DISABLE_MQTT
    else if (is_mqtt(c))
      mqtt_broadcast_aqualinkstate(c);
#endif
  }
  return;
}


void send_mqtt(struct mg_connection *nc, char *toppic, char *message)
{
  static uint16_t msg_id = 0;

  if (toppic == NULL)
    return;

  if (msg_id >= 65535){msg_id=1;}else{msg_id++;}

  //mg_mqtt_publish(nc, toppic, msg_id, MG_MQTT_QOS(0), message, strlen(message));
  mg_mqtt_publish(nc, toppic, msg_id, MG_MQTT_RETAIN | MG_MQTT_QOS(1), message, strlen(message));

  logMessage(LOG_INFO, "MQTT: Published id=%d: %s %s\n", msg_id, toppic, message);
}


void send_domoticz_mqtt_state_msg(struct mg_connection *nc, int idx, int value) 
{
  if (idx <= 0)
    return;

  char mqtt_msg[JSON_MQTT_MSG_SIZE];
  build_mqtt_status_JSON(mqtt_msg ,JSON_MQTT_MSG_SIZE, idx, value, TEMP_UNKNOWN);
  send_mqtt(nc, _aqualink_config->mqtt_dz_pub_topic, mqtt_msg);
}

void send_domoticz_mqtt_temp_msg(struct mg_connection *nc, int idx, int value) 
{
  if (idx <= 0)
    return;

  char mqtt_msg[JSON_MQTT_MSG_SIZE];
  build_mqtt_status_JSON(mqtt_msg ,JSON_MQTT_MSG_SIZE, idx, 0, (_aqualink_data->temp_units==FAHRENHEIT)?roundf(degFtoC(value)):value);
  send_mqtt(nc, _aqualink_config->mqtt_dz_pub_topic, mqtt_msg);
}
void send_domoticz_mqtt_numeric_msg(struct mg_connection *nc, int idx, int value) 
{
  if (idx <= 0)
    return;

  char mqtt_msg[JSON_MQTT_MSG_SIZE];
  build_mqtt_status_JSON(mqtt_msg ,JSON_MQTT_MSG_SIZE, idx, 0, value);
  send_mqtt(nc, _aqualink_config->mqtt_dz_pub_topic, mqtt_msg);
}

void send_mqtt_state_msg(struct mg_connection *nc, char *dev_name, aqledstate state)
{
  static char mqtt_pub_topic[250];

  sprintf(mqtt_pub_topic, "%s/%s",_aqualink_config->mqtt_aq_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, (state==OFF?"0":"1"));
}
void send_mqtt_temp_msg(struct mg_connection *nc, char *dev_name, long value)
{
  static char mqtt_pub_topic[250];
  static char degC[5];
  
  sprintf(degC, "%.2f", (_aqualink_data->temp_units==FAHRENHEIT)?degFtoC(value):value );
  sprintf(mqtt_pub_topic, "%s/%s", _aqualink_config->mqtt_aq_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, degC);
}
void send_mqtt_setpoint_msg(struct mg_connection *nc, char *dev_name, long value)
{
  static char mqtt_pub_topic[250];
  static char degC[5];
  
  sprintf(degC, "%.2f", (_aqualink_data->temp_units==FAHRENHEIT)?degFtoC(value):value );
  sprintf(mqtt_pub_topic, "%s/%s/setpoint", _aqualink_config->mqtt_aq_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, degC);
}
void send_mqtt_numeric_msg(struct mg_connection *nc, char *dev_name, int value)
{
  static char mqtt_pub_topic[250];
  static char msg[10];
  
  sprintf(msg, "%d", value);
  sprintf(mqtt_pub_topic, "%s/%s", _aqualink_config->mqtt_aq_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, msg);
}


void mqtt_broadcast_aqualinkstate(struct mg_connection *nc)
{
  int i;

//logMessage(LOG_INFO, "mqtt_broadcast_aqualinkstate: START\n");

  if (_aqualink_data->air_temp != TEMP_UNKNOWN && _aqualink_data->air_temp != _last_mqtt_aqualinkdata.air_temp) {
    _last_mqtt_aqualinkdata.air_temp = _aqualink_data->air_temp;
    send_mqtt_temp_msg(nc, AIR_TEMP_TOPIC, _aqualink_data->air_temp);
    send_domoticz_mqtt_temp_msg(nc, _aqualink_config->dzidx_air_temp, _aqualink_data->air_temp);
  }
  if (_aqualink_data->pool_temp != TEMP_UNKNOWN && _aqualink_data->pool_temp != _last_mqtt_aqualinkdata.pool_temp) {
    _last_mqtt_aqualinkdata.pool_temp = _aqualink_data->pool_temp;
    send_mqtt_temp_msg(nc, POOL_TEMP_TOPIC, _aqualink_data->pool_temp);
    send_domoticz_mqtt_temp_msg(nc, _aqualink_config->dzidx_pool_water_temp, _aqualink_data->pool_temp);
  }
  if (_aqualink_data->spa_temp != TEMP_UNKNOWN && _aqualink_data->spa_temp != _last_mqtt_aqualinkdata.spa_temp) {
    _last_mqtt_aqualinkdata.spa_temp = _aqualink_data->spa_temp;
    send_mqtt_temp_msg(nc, SPA_TEMP_TOPIC, _aqualink_data->spa_temp);
    send_domoticz_mqtt_temp_msg(nc, _aqualink_config->dzidx_spa_water_temp, _aqualink_data->spa_temp);
  } else if (_aqualink_data->pool_temp != TEMP_UNKNOWN && _aqualink_data->pool_temp != _last_mqtt_aqualinkdata.spa_temp) {
    // Use Pool Temp is Spa is not available
    _last_mqtt_aqualinkdata.spa_temp = _aqualink_data->pool_temp;
    send_mqtt_temp_msg(nc, SPA_TEMP_TOPIC, _aqualink_data->pool_temp);
    send_domoticz_mqtt_temp_msg(nc, _aqualink_config->dzidx_spa_water_temp, _aqualink_data->pool_temp);
  }
  if (_aqualink_data->pool_htr_set_point != TEMP_UNKNOWN && _aqualink_data->pool_htr_set_point != _last_mqtt_aqualinkdata.pool_htr_set_point) {
    _last_mqtt_aqualinkdata.pool_htr_set_point = _aqualink_data->pool_htr_set_point;
    send_mqtt_setpoint_msg(nc, BTN_POOL_HTR, _aqualink_data->pool_htr_set_point);
    // removed until domoticz has a better virtuel thermostat
    //send_domoticz_mqtt_temp_msg(nc, _aqualink_config->dzidx_pool_thermostat, _aqualink_data->pool_htr_set_point);
  }
  if (_aqualink_data->spa_htr_set_point != TEMP_UNKNOWN && _aqualink_data->spa_htr_set_point != _last_mqtt_aqualinkdata.spa_htr_set_point) {
    _last_mqtt_aqualinkdata.spa_htr_set_point = _aqualink_data->spa_htr_set_point;
    send_mqtt_setpoint_msg(nc, BTN_SPA_HTR, _aqualink_data->spa_htr_set_point);
    // removed until domoticz has a better virtuel thermostat
    //send_domoticz_mqtt_temp_msg(nc, _aqualink_config->dzidx_spa_thermostat, _aqualink_data->spa_htr_set_point);
  }
  if (_aqualink_data->frz_protect_set_point != TEMP_UNKNOWN && _aqualink_data->frz_protect_set_point != _last_mqtt_aqualinkdata.frz_protect_set_point) {
    _last_mqtt_aqualinkdata.frz_protect_set_point = _aqualink_data->frz_protect_set_point;
    send_mqtt_setpoint_msg(nc, FREEZE_PROTECT, _aqualink_data->frz_protect_set_point);
    //send_domoticz_mqtt_temp_msg(nc, _aqualink_config->dzidx_rfz_protect, _aqualink_data->frz_protect_set_point);
  }
  if (_aqualink_data->swg_percent != TEMP_UNKNOWN && _aqualink_data->swg_percent != _last_mqtt_aqualinkdata.swg_percent) {
    _last_mqtt_aqualinkdata.swg_percent = _aqualink_data->swg_percent;
    send_mqtt_numeric_msg(nc, SWG_PERCENT_TOPIC, _aqualink_data->swg_percent);
    send_domoticz_mqtt_numeric_msg(nc, _aqualink_config->dzidx_swg_percent, _aqualink_data->swg_percent);
  }
  if (_aqualink_data->swg_ppm != TEMP_UNKNOWN && _aqualink_data->swg_ppm != _last_mqtt_aqualinkdata.swg_ppm) {
    _last_mqtt_aqualinkdata.swg_ppm = _aqualink_data->swg_ppm;
    send_mqtt_numeric_msg(nc, SWG_PPM_TOPIC, _aqualink_data->swg_ppm);
    send_domoticz_mqtt_numeric_msg(nc, _aqualink_config->dzidx_swg_ppm, _aqualink_data->swg_ppm);
  }

//logMessage(LOG_INFO, "mqtt_broadcast_aqualinkstate: START LEDs\n");

//if (time(NULL) % 2) {}   <-- use to determin odd/even second in time to make state flash on enabled.

  // Loop over LED's and send any changes.
  for (i=0; i < TOTAL_BUTTONS; i++) {
    //logMessage(LOG_INFO, "LED %d : new state %d | old state %d\n", i, _aqualink_data->aqbuttons[i].led->state, _last_mqtt_aqualinkdata.aqualinkleds[i].state);
    if (_last_mqtt_aqualinkdata.aqualinkleds[i].state != _aqualink_data->aqbuttons[i].led->state){
      _last_mqtt_aqualinkdata.aqualinkleds[i].state = _aqualink_data->aqbuttons[i].led->state;
      if (_aqualink_data->aqbuttons[i].dz_idx != DZ_NULL_IDX) {
        send_mqtt_state_msg(nc, _aqualink_data->aqbuttons[i].name, _aqualink_data->aqbuttons[i].led->state);
        send_domoticz_mqtt_state_msg(nc, _aqualink_data->aqbuttons[i].dz_idx, (_aqualink_data->aqbuttons[i].led->state==OFF?DZ_OFF:DZ_ON));
      }
      // Send mqtt
    }
  }
  //logMessage(LOG_INFO, "mqtt_broadcast_aqualinkstate: END\n");
}


// 

int getTempforMeteohub(char *buffer)
{
  int length = 0;
  
  if (_aqualink_data->air_temp != TEMP_UNKNOWN)
    length += sprintf(buffer+length, "t0 %d\n",(int)degFtoC(_aqualink_data->air_temp)*10);
  else
    length += sprintf(buffer+length, "t0 \n");
    
  if (_aqualink_data->pool_temp != TEMP_UNKNOWN)
    length += sprintf(buffer+length, "t1 %d\n",(int)degFtoC(_aqualink_data->pool_temp)*10);
  else
    length += sprintf(buffer+length, "t1 \n");
    
  return strlen(buffer);
}

void set_light_mode(char *value) 
{
  char buf[20];
  // 5 below is light index, need to look this up so it's not hard coded.
  sprintf(buf, "%-5s%-5d%.2f",value, 5, _aqualink_config->light_programming_mode );
  aq_programmer(AQ_SET_COLORMODE, buf, _aqualink_data);
}

void action_web_request(struct mg_connection *nc, struct http_message *http_msg) {
  // struct http_message *http_msg = (struct http_message *)ev_data;
  if (getLogLevel() >= LOG_INFO) { // Simply for log message, check we are at
                                   // this log level before running all this
                                   // junk
    char *uri = (char *)malloc(http_msg->uri.len + http_msg->query_string.len + 2);
    strncpy(uri, http_msg->uri.p, http_msg->uri.len + http_msg->query_string.len + 1);
    uri[http_msg->uri.len + http_msg->query_string.len + 1] = '\0';
    logMessage(LOG_INFO, "URI request: '%s'\n", uri);
    free(uri);
  }
  // If we have a get request, pass it
  if (strstr(http_msg->method.p, "GET") && http_msg->query_string.len > 0) {
    char command[20];

    mg_get_http_var(&http_msg->query_string, "command", command, sizeof(command));
    logMessage(LOG_INFO, "WEB: Message command='%s'\n", command);

    // if (strstr(http_msg->query_string.p, "command=status")) {
    if (strcmp(command, "status") == 0) {
      char data[JSON_STATUS_SIZE];
      int size = build_aqualink_status_JSON(_aqualink_data, data, JSON_STATUS_SIZE);
      mg_send_head(nc, 200, size, "Content-Type: application/json");
      mg_send(nc, data, size);

      //} else if (strstr(http_msg->query_string.p, "command=mhstatus")) {
    } else if (strcmp(command, "mhstatus") == 0) {
      char data[20];
      int size = getTempforMeteohub(data);
      mg_send_head(nc, 200, size, "Content-Type: text/plain");
      mg_send(nc, data, size);

    } else if (strcmp(command, "poollightmode") == 0) {
      char value[20];
      mg_get_http_var(&http_msg->query_string, "value", value, sizeof(value));
      //aq_programmer(AQ_SET_COLORMODE, value, _aqualink_data);
      set_light_mode(value);
      mg_send_head(nc, 200, strlen(GET_RTN_OK), "Content-Type: text/plain");
      mg_send(nc, GET_RTN_OK, strlen(GET_RTN_OK));

    } else if (strcmp(command, "diag") == 0) {
      aq_programmer(AQ_GET_DIAGNOSTICS_MODEL, NULL, _aqualink_data);
      mg_send_head(nc, 200, strlen(GET_RTN_OK), "Content-Type: text/plain");
      mg_send(nc, GET_RTN_OK, strlen(GET_RTN_OK));

    } else if (strcmp(command, "POOL_HTR") == 0) {
      char value[20];
      mg_get_http_var(&http_msg->query_string, "value", value, sizeof(value));
      aq_programmer(AQ_SET_POOL_HEATER_TEMP, value, _aqualink_data);
      mg_send_head(nc, 200, strlen(GET_RTN_OK), "Content-Type: text/plain");
      mg_send(nc, GET_RTN_OK, strlen(GET_RTN_OK));

    } else if (strcmp(command, "SPA_HTR") == 0) {
      char value[20];
      mg_get_http_var(&http_msg->query_string, "value", value, sizeof(value));
      aq_programmer(AQ_SET_SPA_HEATER_TEMP, value, _aqualink_data);
      mg_send_head(nc, 200, strlen(GET_RTN_OK), "Content-Type: text/plain");
      mg_send(nc, GET_RTN_OK, strlen(GET_RTN_OK));

    } else if (strcmp(command, "FRZ_PROTECT") == 0) {
      char value[20];
      mg_get_http_var(&http_msg->query_string, "value", value, sizeof(value));
      aq_programmer(AQ_SET_FRZ_PROTECTION_TEMP, value, _aqualink_data);
      mg_send_head(nc, 200, strlen(GET_RTN_OK), "Content-Type: text/plain");
      mg_send(nc, GET_RTN_OK, strlen(GET_RTN_OK));

      //} else if (strstr(http_msg->query_string.p, "command=mhstatus")) {
    } else {
      int i;
      for (i = 0; i < TOTAL_BUTTONS; i++) {
        if (strcmp(command, _aqualink_data->aqbuttons[i].name) == 0) {
          char value[20];
          char *rtn;
          mg_get_http_var(&http_msg->query_string, "value", value, sizeof(value));
          // logMessage (LOG_INFO, "Web Message command='%s'\n",command);
          // aq_programmer(AQ_SEND_CMD, (char
          // *)&_aqualink_data->aqbuttons[i].code, _aqualink_data);
          logMessage(LOG_DEBUG, "WEB: Message request '%s' change state to '%s'\n", command, value);

          if (strcmp(value, "on") == 0) {
            if (_aqualink_data->aqbuttons[i].led->state == OFF || _aqualink_data->aqbuttons[i].led->state == FLASH) {
              aq_programmer(AQ_SEND_CMD, (char *)&_aqualink_data->aqbuttons[i].code, _aqualink_data);
              rtn = GET_RTN_OK;
              logMessage(LOG_INFO, "WEB: turn ON '%s' changed state to '%s'\n", command, value);
            } else {
              rtn = GET_RTN_NOT_CHANGED;
              logMessage(LOG_INFO, "WEB: '%s' is already on '%s', current state %d\n", command, value, _aqualink_data->aqbuttons[i].led->state);
            }
          } else if (strcmp(value, "off") == 0) {
            if (_aqualink_data->aqbuttons[i].led->state == ON || 
                _aqualink_data->aqbuttons[i].led->state == ENABLE ||
                _aqualink_data->aqbuttons[i].led->state == FLASH) {
              aq_programmer(AQ_SEND_CMD, (char *)&_aqualink_data->aqbuttons[i].code, _aqualink_data);
              rtn = GET_RTN_OK;
              logMessage(LOG_INFO, "WEB: turn Off '%s' changed state to '%s'\n", command, value);
            } else {
              rtn = GET_RTN_NOT_CHANGED;
              logMessage(LOG_INFO, "WEB: '%s' is already off '%s', current state %d\n", command, value, _aqualink_data->aqbuttons[i].led->state);
            }
          } else { // Blind switch
            aq_programmer(AQ_SEND_CMD, (char *)&_aqualink_data->aqbuttons[i].code, _aqualink_data);
            rtn = GET_RTN_OK;
            logMessage(LOG_INFO, "WEB: '%s' blindly changed state\n", command, value);
          }

          logMessage(LOG_DEBUG, "WEB: On=%d, Off=%d, Enable=%d, Flash=%d\n", ON, OFF, ENABLE, FLASH);
          // NSF change OK and 2 below to a constant
          mg_send_head(nc, 200, strlen(rtn), "Content-Type: text/plain");
          mg_send(nc, rtn, strlen(rtn));

          // NSF place check we found command here
        }
      }
    }
    // If we get here, got a bad query
    mg_send_head(nc, 200, sizeof(GET_RTN_UNKNOWN), "Content-Type: text/plain");
    mg_send(nc, GET_RTN_UNKNOWN, sizeof(GET_RTN_UNKNOWN));

  } else {
    struct mg_serve_http_opts opts;
    memset(&opts, 0, sizeof(opts)); // Reset all options to defaults
    opts.document_root = _web_root; // Serve files from the current directory
    // logMessage (LOG_DEBUG, "Doc root=%s\n",opts.document_root);
    mg_serve_http(nc, http_msg, s_http_server_opts);
  }
}

void action_websocket_request(struct mg_connection *nc, struct websocket_message *wm) {
  char buffer[50];
  struct JSONwebrequest request;

  strncpy(buffer, (char *)wm->data, wm->size);
  buffer[wm->size] = '\0';
  // logMessage (LOG_DEBUG, "buffer '%s'\n", buffer);

  parseJSONwebrequest(buffer, &request);
  logMessage(LOG_INFO, "WS: Message - Key '%s' Value '%s' | Key2 '%s' Value2 '%s'\n", request.first.key, request.first.value, request.second.key,
             request.second.value);

  if (strcmp(request.first.key, "command") == 0) {
    if (strcmp(request.first.value, "GET_AUX_LABELS") == 0) {
      char labels[JSON_LABEL_SIZE];
      build_aux_labels_JSON(_aqualink_data, labels, JSON_LABEL_SIZE);
      ws_send(nc, labels);
    } else { // Search for value in command list
      int i;
      for (i = 0; i < TOTAL_BUTTONS; i++) {
        if (strcmp(request.first.value, _aqualink_data->aqbuttons[i].name) == 0) {
          logMessage (LOG_INFO, "WS: button '%s' pressed\n",_aqualink_data->aqbuttons[i].name);
          // send_command( (unsigned char)_aqualink_data->aqbuttons[i].code);
          aq_programmer(AQ_SEND_CMD, (char *)&_aqualink_data->aqbuttons[i].code, _aqualink_data);
          break;
          // NSF place check we found command here
        }
      }
    }
  } else if (strcmp(request.first.key, "parameter") == 0) {
    if (strcmp(request.first.value, "FRZ_PROTECT") == 0) {
      aq_programmer(AQ_SET_FRZ_PROTECTION_TEMP, request.second.value, _aqualink_data);
    } else if (strcmp(request.first.value, "POOL_HTR") == 0) {
      aq_programmer(AQ_SET_POOL_HEATER_TEMP, request.second.value, _aqualink_data);
    } else if (strcmp(request.first.value, "SPA_HTR") == 0) {
      aq_programmer(AQ_SET_SPA_HEATER_TEMP, request.second.value, _aqualink_data);
    } else if (strcmp(request.first.value, "POOL_LIGHT_MODE") == 0) {
      //aq_programmer(AQ_SET_COLORMODE, request.second.value, _aqualink_data);
      set_light_mode(request.second.value);
    } else {
      logMessage(LOG_DEBUG, "WS: Unknown parameter %s\n", request.first.value);
    }
  }
}

void action_mqtt_message(struct mg_connection *nc, struct mg_mqtt_message *msg) {
  int i;
  //logMessage(LOG_DEBUG, "MQTT: topic %.*s %.2f\n",msg->topic.len, msg->topic.p, atof(msg->payload.p));
  logMessage(LOG_DEBUG, "MQTT: topic %.*s %.*s\n",msg->topic.len, msg->topic.p, msg->payload.len, msg->payload.p);

  //Need to do this in a better manor, but for present it's ok.
  static char tmp[20];
  strncpy(tmp, msg->payload.p, msg->payload.len);
  tmp[msg->payload.len] = '\0';

  float value = atof(tmp);
  //logMessage(LOG_DEBUG, "MQTT: topic converted %.*s %s %.2f\n",msg->topic.len, msg->topic.p, tmp, value);

//printf("Topic %.*s\n",msg->topic.len, msg->topic.p);
  // get the parts from the topic
  char *pt1 = (char *)&msg->topic.p[strlen(_aqualink_config->mqtt_aq_topic)+1];
  char *pt2 = NULL;
  char *pt3 = NULL;

  for (i=10; i < msg->topic.len; i++) {
    if ( msg->topic.p[i] == '/' ) {
      if (pt2 == NULL) {
        pt2 = (char *)&msg->topic.p[++i];
      } else if (pt3 == NULL) {
        pt3 = (char *)&msg->topic.p[++i];
        break;
      }
    }
  }

  //logMessage(LOG_DEBUG, "MQTT: topic %.*s %.2f\n",msg->topic.len, msg->topic.p, atof(msg->payload.p));
  //only care about topics with set at the end.
  //aqualinkd/Freeze/setpoint/set
  //aqualinkd/Filter_Pump/set
  //aqualinkd/Pool_Heater/setpoint/set
  //aqualinkd/Pool_Heater/set

  if (pt3 != NULL && (strncmp(pt2, "setpoint", 8) == 0) && (strncmp(pt3, "set", 3) == 0)) {
    int val = _aqualink_data->unactioned.value = (_aqualink_data->temp_units == FAHRENHEIT) ? round(degCtoF(value)) : round(value);
    if (strncmp(pt1, BTN_POOL_HTR, strlen(BTN_POOL_HTR)) == 0) {
      if (val <= HEATER_MAX && val >= MEATER_MIN) {
        logMessage(LOG_INFO, "MQTT: request to set pool heater setpoint to %.2fc\n", value);
        _aqualink_data->unactioned.type = POOL_HTR_SETOINT;
      } else {
        logMessage(LOG_ERR, "MQTT: request to set pool heater setpoint to %.2fc is outside of range\n", value);
        send_mqtt_setpoint_msg(nc, BTN_POOL_HTR, _aqualink_data->pool_htr_set_point);
      }
    } else if (strncmp(pt1, BTN_SPA_HTR, strlen(BTN_SPA_HTR)) == 0) {
      if (val <= HEATER_MAX && val >= MEATER_MIN) {
        logMessage(LOG_INFO, "MQTT: request to set spa heater setpoint to %.2fc\n", value);
        _aqualink_data->unactioned.type = SPA_HTR_SETOINT;
      } else {
        logMessage(LOG_ERR, "MQTT: request to set spa heater setpoint to %.2fc is outside of range\n", value);
        send_mqtt_setpoint_msg(nc, BTN_SPA_HTR, _aqualink_data->spa_htr_set_point);
      }
    } else if (strncmp(pt1, FREEZE_PROTECT, strlen(FREEZE_PROTECT)) == 0) {
      if (val <= FREEZE_PT_MAX && val >= FREEZE_PT_MIN) {
      logMessage(LOG_INFO, "MQTT: request to set freeze protect to %.2fc\n", value);
      _aqualink_data->unactioned.type = FREEZE_SETPOINT;
      } else {
        logMessage(LOG_ERR, "MQTT: request to set freeze protect to %.2fc is outside of range\n", value);
      }
    } else {
      // Not sure what the setpoint is, ignore.
      logMessage(LOG_DEBUG, "MQTT: ignoring %.*s don't recognise button setpoint\n", msg->topic.len, msg->topic.p);
      return;
    }
    // logMessage(LOG_INFO, "MQTT: topic %.*s %.2f, setting %s\n",msg->topic.len, msg->topic.p, value);
    time(&_aqualink_data->unactioned.requested);
    
  } else if (pt2 != NULL && (strncmp(pt2, "set", 3) == 0) && (strncmp(pt2, "setpoint", 8) != 0)) {
    // Must be a switch on / off
    for (i=0; i < TOTAL_BUTTONS; i++) {
      if (strncmp(pt1, _aqualink_data->aqbuttons[i].name, strlen(_aqualink_data->aqbuttons[i].name)) == 0 ){
        logMessage(LOG_INFO, "MQTT: MATCH %s to topic %.*s\n",_aqualink_data->aqbuttons[i].name,msg->topic.len, msg->topic.p);
        // Message is either a 1 or 0 for on or off
        //int status = atoi(msg->payload.p);
        if ( value > 1 || value < 0) {
          logMessage(LOG_ERR, "MQTT: topic %.*s %.2f\n",msg->topic.len, msg->topic.p, value);
          logMessage(LOG_ERR, "MQTT: received unknown status of '%.*s' for '%s', Ignoring!\n", msg->payload.len, msg->payload.p, _aqualink_data->aqbuttons[i].name);
        }
        else if ( (_aqualink_data->aqbuttons[i].led->state == OFF && value==0) ||
           (value == 1 && (_aqualink_data->aqbuttons[i].led->state == ON || 
                                _aqualink_data->aqbuttons[i].led->state == FLASH || 
                                _aqualink_data->aqbuttons[i].led->state == ENABLE))) {
          logMessage(LOG_INFO, "MQTT: received '%s' for '%s', already '%s', Ignoring\n", (value==0?"OFF":"ON"), _aqualink_data->aqbuttons[i].name, (value==0?"OFF":"ON"));
        } else {
          logMessage(LOG_INFO, "MQTT: received '%s' for '%s', turning '%s'\n", (value==0?"OFF":"ON"), _aqualink_data->aqbuttons[i].name,(value==0?"OFF":"ON"));
          aq_programmer(AQ_SEND_CMD, (char *)&_aqualink_data->aqbuttons[i].code, _aqualink_data);
        }
        break;
      }
    }
  } else {
    // We don's care
    logMessage(LOG_DEBUG, "MQTT: ignoring topic %.*s\n",msg->topic.len, msg->topic.p);
  }
}

void action_domoticz_mqtt_message(struct mg_connection *nc, struct mg_mqtt_message *msg) {
  int idx = -1;
  int nvalue = -1;
  int i;
  char svalue[DZ_SVALUE_LEN];

  if (parseJSONmqttrequest(msg->payload.p, msg->payload.len, &idx, &nvalue, svalue)) {
    logMessage(LOG_DEBUG, "MQTT: DZ: Received message IDX=%d nValue=%d sValue=%s\n", idx, nvalue, svalue);
    for (i=0; i < TOTAL_BUTTONS; i++) {
      if (_aqualink_data->aqbuttons[i].dz_idx == idx){
        //NSF, should try to simplify this if statment, but not easy since AQ ON and DZ ON are different, and AQ has other states.
        if ( (_aqualink_data->aqbuttons[i].led->state == OFF && nvalue==DZ_OFF) ||
           (nvalue == DZ_ON && (_aqualink_data->aqbuttons[i].led->state == ON || 
                                _aqualink_data->aqbuttons[i].led->state == FLASH || 
                                _aqualink_data->aqbuttons[i].led->state == ENABLE))) {
          logMessage(LOG_INFO, "MQTT: DZ: received '%s' for '%s', already '%s', Ignoring\n", (nvalue==DZ_OFF?"OFF":"ON"), _aqualink_data->aqbuttons[i].name, (nvalue==DZ_OFF?"OFF":"ON"));
        } else {
          // NSF Below if needs to check that the button pressed is actually a light. Add this later
          if (_aqualink_data->active_thread.ptype == AQ_SET_COLORMODE ) {
            logMessage(LOG_NOTICE, "MQTT: DZ: received '%s' for '%s', IGNORING as we are programming light mode\n", (nvalue==DZ_OFF?"OFF":"ON"), _aqualink_data->aqbuttons[i].name);
          } else {
            logMessage(LOG_INFO, "MQTT: DZ: received '%s' for '%s', turning '%s'\n", (nvalue==DZ_OFF?"OFF":"ON"), _aqualink_data->aqbuttons[i].name,(nvalue==DZ_OFF?"OFF":"ON"));
            aq_programmer(AQ_SEND_CMD, (char *)&_aqualink_data->aqbuttons[i].code, _aqualink_data);
          }
        }
        break; // no need to continue in for loop, we found button.
      }
    }
    /* removed until domoticz has a better virtual thermostat
    if (idx == _aqualink_config->dzidx_pool_thermostat) {
      float degC = atof(svalue);
      int degF = (int)degCtoF(degC);
      if (degC > 0.0 && 1 < (degF - _aqualink_data->pool_htr_set_point)) {
        logMessage(LOG_INFO, "MQTT: DZ: received temp setting '%s' for 'pool heater setpoint' old value %d(f) setting to %f(c) %d(f)\n",svalue, _aqualink_data->pool_htr_set_point, degC, degF);
        //aq_programmer(AQ_SET_POOL_HEATER_TEMP, (int)degCtoF(degl), _aqualink_data);
      } else {
        logMessage(LOG_INFO, "MQTT: DZ: received temp setting '%s' for 'pool heater setpoint' matched current setting (or was zero), ignoring!\n", svalue);
      }
    } else if (idx == _aqualink_config->dzidx_spa_thermostat) {
      float degC = atof(svalue);
      int degF = (int)degCtoF(degC);
      if (degC > 0.0 && 1 < (degF - _aqualink_data->spa_htr_set_point)) {
        logMessage(LOG_INFO, "MQTT: DZ: received temp setting '%s' for 'spa heater setpoint' old value %d(f) setting to %f(c) %d(f)\n",svalue, _aqualink_data->spa_htr_set_point, degC, degF);
        //aq_programmer(AQ_SET_POOL_HEATER_TEMP, (int)degCtoF(degl), _aqualink_data);
      } else {
        logMessage(LOG_INFO, "MQTT: DZ: received temp setting '%s' for 'spa heater setpoint' matched current setting (or was zero), ignoring!\n", svalue);
      }
    }*/
//printf("Finished checking IDX for setpoint\n"); 
  }

  // NSF Need to check idx against ours and decide if we are interested in it.
}

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
  struct mg_mqtt_message *mqtt_msg;
  struct http_message *http_msg;
  struct websocket_message *ws_msg;
  //static double last_control_time;

  // logMessage (LOG_DEBUG, "Event\n");
  switch (ev) {
  case MG_EV_HTTP_REQUEST:
    //nc->user_data = WEB;
    http_msg = (struct http_message *)ev_data;
    action_web_request(nc, http_msg);
    break;
  
  case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
    //nc->user_data = WS;
    logMessage(LOG_DEBUG, "++ Websocket joined\n");
    break;
  
  case MG_EV_WEBSOCKET_FRAME: 
    ws_msg = (struct websocket_message *)ev_data;
    action_websocket_request(nc, ws_msg);
    break;
  
  case MG_EV_CLOSE: 
    if (is_websocket(nc)) {
      logMessage(LOG_DEBUG, "-- Websocket left\n");
    } else if (is_mqtt(nc)) {
      logMessage(LOG_WARNING, "MQTT Connection closed\n");
      _mqtt_exit_flag = true;
    }
    break;
  
  case MG_EV_CONNECT: {
    //nc->user_data = MQTT;
    //nc->flags |= MG_F_USER_1; // NFS Need to readup on this
    set_mqtt(nc);
    _mqtt_exit_flag = false;
    //char *MQTT_id = "AQUALINK_MQTT_TEST_ID";
    struct mg_send_mqtt_handshake_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.user_name = _aqualink_config->mqtt_user;
    opts.password = _aqualink_config->mqtt_passwd;
    opts.keep_alive = 5;
    opts.flags |= MG_MQTT_CLEAN_SESSION; // NFS Need to readup on this
    mg_set_protocol_mqtt(nc);
    mg_send_mqtt_handshake_opt(nc, _aqualink_config->mqtt_ID, opts);
    logMessage(LOG_INFO, "MQTT: Subscribing mqtt with id of: %s\n", _aqualink_config->mqtt_ID);
    //last_control_time = mg_time();
  } break;

  case MG_EV_MQTT_CONNACK:
    {
      struct mg_mqtt_topic_expression topics[2];
      char aq_topic[30];
      int qos=0;// can't be bothered with ack, so set to 0

      logMessage(LOG_INFO, "MQTT: Connection acknowledged\n");
      mqtt_msg = (struct mg_mqtt_message *)ev_data;
      if (mqtt_msg->connack_ret_code != MG_EV_MQTT_CONNACK_ACCEPTED) {
        logMessage(LOG_WARNING, "Got mqtt connection error: %d\n", mqtt_msg->connack_ret_code);
        _mqtt_exit_flag = true;
      }
      
      snprintf(aq_topic, 29, "%s/#", _aqualink_config->mqtt_aq_topic);
      if (_aqualink_config->mqtt_aq_topic != NULL && _aqualink_config->mqtt_dz_sub_topic != NULL) {
        topics[0].topic = aq_topic;
        topics[0].qos = qos; 
        topics[1].topic = _aqualink_config->mqtt_dz_sub_topic;
        topics[1].qos = qos;
        mg_mqtt_subscribe(nc, topics, 2, 42);
        logMessage(LOG_INFO, "MQTT: Subscribing to '%s'\n", aq_topic);
        logMessage(LOG_INFO, "MQTT: Subscribing to '%s'\n", _aqualink_config->mqtt_dz_sub_topic);
      } 
      else if (_aqualink_config->mqtt_aq_topic != NULL) {
        topics[0].topic = aq_topic;
        topics[0].qos = qos;
        mg_mqtt_subscribe(nc, topics, 1, 42);
        logMessage(LOG_INFO, "MQTT: Subscribing to '%s'\n", aq_topic);
      } 
      else if (_aqualink_config->mqtt_dz_sub_topic != NULL) {
        topics[0].topic = _aqualink_config->mqtt_dz_sub_topic;;
        topics[0].qos = qos;
        mg_mqtt_subscribe(nc, topics, 1, 42);
        logMessage(LOG_INFO, "MQTT: Subscribing to '%s'\n", _aqualink_config->mqtt_dz_sub_topic);
      }
    }
    break;
  case MG_EV_MQTT_PUBACK:
    mqtt_msg = (struct mg_mqtt_message *)ev_data;
    logMessage(LOG_INFO, "MQTT: Message publishing acknowledged (msg_id: %d)\n", mqtt_msg->message_id);
    break;
  case MG_EV_MQTT_SUBACK:
    logMessage(LOG_INFO, "MQTT: Subscription(s) acknowledged\n");
    break;
  case MG_EV_MQTT_PUBLISH: 
    mqtt_msg = (struct mg_mqtt_message *)ev_data;
    
    if (mqtt_msg->message_id != 0) {
      logMessage(LOG_INFO, "MQTT: received (msg_id: %d), looks like my own message, ignoring\n", mqtt_msg->message_id);
    }
// NSF Need to change strlen to a global so it's not executed every time we check a topic
    if (_aqualink_config->mqtt_aq_topic != NULL && strncmp(mqtt_msg->topic.p, _aqualink_config->mqtt_aq_topic, strlen(_aqualink_config->mqtt_aq_topic)) == 0) {
      action_mqtt_message(nc, mqtt_msg);
    }
    if (_aqualink_config->mqtt_dz_sub_topic != NULL && strncmp(mqtt_msg->topic.p, _aqualink_config->mqtt_dz_sub_topic, strlen(_aqualink_config->mqtt_dz_sub_topic)) == 0) {
      action_domoticz_mqtt_message(nc, mqtt_msg);
    }
    break;
    /*
    // MQTT ping wasn't working in previous versions.
  case MG_EV_POLL: {
    struct mg_mqtt_proto_data *pd = (struct mg_mqtt_proto_data *)nc->proto_data;
    double now = mg_time();

    if (pd->keep_alive > 0 && last_control_time > 0 && (now - last_control_time) > pd->keep_alive) {
      logMessage(LOG_INFO, "MQTT: Sending MQTT ping\n");
      mg_mqtt_ping(nc);
      last_control_time = now;
    }
  }
    break;*/
  }
}

void start_mqtt(struct mg_mgr *mgr) {
  logMessage (LOG_NOTICE, "Starting MQTT client to %s\n", _aqualink_config->mqtt_server);
  if ( _aqualink_config->mqtt_server == NULL || 
      ( _aqualink_config->mqtt_aq_topic == NULL && _aqualink_config->mqtt_dz_pub_topic == NULL && _aqualink_config->mqtt_dz_sub_topic == NULL) )
    return;

  if (mg_connect(mgr, _aqualink_config->mqtt_server, ev_handler) == NULL) {
      logMessage (LOG_ERR, "Failed to create MQTT listener to %s\n", _aqualink_config->mqtt_server);
  } else {
    int i;
    for (i=0; i < TOTAL_BUTTONS; i++) {
      _last_mqtt_aqualinkdata.aqualinkleds[i].state = LED_S_UNKNOWN;
    }
    _mqtt_exit_flag = false; // set here to stop multiple connects, if it fails truley fails it will get set to false.
  }
}

//bool start_web_server(struct mg_mgr *mgr, struct aqualinkdata *aqdata, char *port, char* web_root) {
bool start_net_services(struct mg_mgr *mgr, struct aqualinkdata *aqdata, struct aqconfig *aqconfig) {
  struct mg_connection *nc;
  _aqualink_data = aqdata;
  _aqualink_config = aqconfig;
 
  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);
  setvbuf(stdout, NULL, _IOLBF, 0);
  setvbuf(stderr, NULL, _IOLBF, 0);
  
  mg_mgr_init(mgr, NULL);
  logMessage (LOG_NOTICE, "Starting web server on port %s\n", _aqualink_config->socket_port);
  nc = mg_bind(mgr, _aqualink_config->socket_port, ev_handler);
  if (nc == NULL) {
    logMessage (LOG_ERR, "Failed to create listener\n");
    return false;
  }

  // Set up HTTP server parameters
  mg_set_protocol_http_websocket(nc);
  s_http_server_opts.document_root = _aqualink_config->web_directory;  // Serve current directory
  s_http_server_opts.enable_directory_listing = "yes";
  
#ifndef MG_DISABLE_MQTT
  // Start MQTT
  start_mqtt(mgr);
#endif

  return true;
}
