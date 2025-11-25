var _confighelp = {};
_confighelp["listen_address"]="Address for webserver to listen to eg http://0.0.0.0:80 To accept all HTTP requests. Do not change unless you know exactly what you are doing. HTTPS is restricted for two way auth only"
_confighelp["https_cert_dir"]="For two way auth only. Directory where crt.pem, key.pem & optional ca.pem are located"
_confighelp["serial_port"]="Device of your RS465 adapter, examples /dev/ttyUSB1, /dev/ttyS2, /dev/serial0"
_confighelp["panel_type"]="Your RS panel type & size. ie 4, 6, 8, 12 or 16 relates to RS4, RS6, RS8, RS12 or RS16. Must be in format XX-N ZZZZ (XX=RS or PD, N=Circuits, ZZZ=Combo or Only or Dual)";
_confighelp["device_id"]="The id of the AqualinkD to use, use serial_logger to find ID's. If your panel is a PDA only model then PDA device ID is 0x60. set to device_id to 0xFF for to autoconfigure all this section";
_confighelp["mqtt_address"]="MQTT address has to be set to ip:port enable MQTT ig mqtt://192.168.1.99:1883"
_confighelp["read_RS485_swg"]="Read device information directly from RS485 bus"
_confighelp["force_swg"]="Force any devices to be active at startup. Must set these for Home Assistant integration"
_confighelp["enable_scheduler"]="AqualinkD's internal scheduler"
_confighelp["event_check_use_scheduler_times"]="Turn on filter pump from events that can cause it to turn off"
_confighelp["sync_panel_time"]="Keep panel time synced with computer"
_confighelp["ftdi_low_latency"]="Give RS485 adapter higher priority in kernel (FTDI chips only)"
_confighelp["rs485_frame_delay"]="Time for AqualinkD to reply to RS485 messages"
_confighelp["light_programming_mode"]="Valid only for AqualinkD programming light color (button_??_light_mode = 0)"
//_confighelp["light_program_01"]="Light colors for AqualinkD programmed lights ie (button_??_light_mode = 0)"