
#ifndef AQ_MONGOOSE_ADDITIONAL_H_
#define AQ_MONGOOSE_ADDITIONAL_H_


/* Flags left for application */
/*.  mongoose 6.x had the below that we used & depend on.
#define MG_F_USER_1 (1 << 20)
#define MG_F_USER_2 (1 << 21)
#define MG_F_USER_3 (1 << 22)
#define MG_F_USER_4 (1 << 23)
#define MG_F_USER_5 (1 << 24)
#define MG_F_USER_6 (1 << 25)


struct mg_connection {
  ....
  ...
  ..
  unsigned long flags;
}
*/


#define MG_F_USER_1 (1 << 0)
#define MG_F_USER_2 (1 << 1)
#define MG_F_USER_3 (1 << 2)
#define MG_F_USER_4 (1 << 3)
#define MG_F_USER_5 (1 << 4)
#define MG_F_USER_6 (1 << 5)


#define AQ_MG_CON_MQTT     MG_F_USER_1

//#define AQ_MG_CON_WS // Plane web interface websocket.
#define AQ_MG_CON_WS_SIM   MG_F_USER_2
#define AQ_MG_CON_WS_AQM   MG_F_USER_3
#define AQ_MG_CON_MQTT_CONNECTING  MG_F_USER_4
#define AQ_MG_CON_MQTT_OPEN  MG_F_USER_5  // MQTT CONNACK received; safe to send PINGREQ

/*
In mongose.h about line 1673 make sure to add aq_flags to the mg_connection strut

struct mg_connection {
  unsigned short aq_flags;
}
*/

#endif //AQ_MONGOOSE_ADDITIONAL_H_