
#ifndef RS485MON_H_
#define RS485MON_H_

/*
int logPackets = PACKET_MAX;
  int logLevel = LOG_NOTICE;
  bool panleProbe = true;
  bool rsSerialSpeedTest = false;
  //bool serialBlocking = true;
  bool errorMonitor = false;
*/
//int rs485mon(int rs_fd, char *port_name);
//int rs485mon(int rs_fd, char *port_name, int logPackets, int logLevel, bool panleProbe, bool rsSerialSpeedTest, bool errorMonitor);

//int rs485mon (int rs_fd, char *port_name, int logLevel);
int rs485mon (int rs_fd, char *port_name, int logLevel, int slogger_packets, char *slogger_ids);
void getPanelInfo(int rs_fd, unsigned char *packet_buffer, int packet_length);

#endif // SERIAL_LOGGER_H_