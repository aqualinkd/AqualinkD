# AqualinkD Release Notes & Changelog

All notable changes to AqualinkD are documented here. Releases are listed in reverse chronological order (newest first).

---

## Release 3.0.3 (Development)
* Fixed setting SWG for PDA v1.2
* Fixed bug with slider run times in web UI
* Fixed bug with heater max min in web UI

---

## Release 3.0.2
* Fixed bug with SWG being enabled if one is not present.
* Fixed bug with light_programs ending in 'show' aqualinkd.conf

---

## Release 3.0.1
* UI Update for web config.
* UI Now support for themes. (auto, dark, light -or- custom)
* Fixed UI not updating for sensors.
* Updates to UOM's for HA.   gal/min, W, rpm
* Fix issue with multiple bad sensors in config.

---

## Release 3.0.0

**⚠️ NOTE: When upgrading to v3.0.0** - if you see blank AqualinkD screen (ie no buttons), please clear browser cache. This also goes for the mobile/webapp (you may need to delete and re-add to mobile home screen).

* Serial optimization for AqualinkD HAT.
* Can now edit webconfig in aqmanager, added many UI customization options.
  * [Click for details](https://github.com/aqualinkd/AqualinkD/wiki#Web-Config)
  * web/config.js is now web/config.json any custom settings will need to be migrated.
  * Added example plugin of how to get HomeAssistant devices to show up in AqualinkD UI. [Click for details](https://github.com/aqualinkd/AqualinkD/wiki#WebUIplugin)
* Upgraded network library (HTTP(S), MQTT(S), WS)
* Added support for HTTPS and MQTTS.
  * HTTPS is for two way auth only, ie You create your own cert and load on both AqualinkD server and all client devices.
  * Example script to generate HTTPS certificates is in (./extras/generate-certs.sh)
* Optimized updates to MQTT, web sockets & WebUI (only update when absolutely necessary)
* Added option to select version to install, including dev releases.
* MQTT Discovery for all supporting hubs (HomeAssistant Domoticz Hubitat OpenHAB etc)
* Moved Domoticz support over to MQTT autodiscovery.
* Change tile color & label for ph / orp & ppm tiles when values are out of optimal range.
* Add mV as the UOM for ORP tile.
* Fixed bug with sensor UOM.
* UI code cleanup.
* Included program advance to AqualinkD programmable light mode. (lot quicker for lights that remember state)
* Changed caching of HTTP server. (Better for UI config updates)
* Autoconfigure will now get panel size/type for panels that support PC-Dock interface.
* Autoconfigure will *try* to work for PDA panels.
* PDA Color light fixes.
* PDA Dimmer lights now supported.
* Cleaned up exit & errors when running as daemon and docker.
* Fixed issues with external sensors and homekit.
* Added preliminary support for Jandy Infinite water color lights.
* Renamed serial_logger to rs485mon (more compliant with unix naming standards)

---

## Release 2.6.11 (September 14, 2025)
* Cleaned up exit codes.
* Fixed delay in shutting down when no data received on RS485
* Updated serial_logger to detect & overcome Jandy panel firmware issues

---

## Release 2.6.10 (September 1, 2025)
* UI now supports UOM for external sensors
* Updates to VSP status
* Fix for Jandy Panel Rev T dimmer lights bug.
* Autoconfig changes for older panels.

---

## Release 2.6.9 (July 26, 2025)
* Config fixes for 0x33 ID / PDA
* Changes to virtual buttons and light modes
* Updates to aqmanager & config editor
* Updates to jandy device logging & heatpump performance.
* Increased performance of external sensors
* Added unit of measure for External sensors
* External sensors now have regexp support (good for onewire devices)

---

## Release 2.6.8 (June 29, 2025)
* Fixed some UI bugs, added download config option
* Changes to config options & config editor
* Heatpump / chiller updates
* PDA updates (detect temperature units, other small changes)

---

## Release 2.6.7 (May 23, 2025)
* Fixed bug with iaqualink protocol when no virtual buttons configured.
* Updated RS timing debug messages.

---

## Release 2.6.6 (May 23, 2025)
* Fixed some HTTP response codes.
* Added checks for protocols vs panel revision.
* Fixed auto_configure for panel REV I & K.
* Updates to install scripts.
* Update to WebUI dimmers.

---

## Release 2.6.5 (May 5, 2025)
* Changes to virtual buttons.

---

## Release 2.6.4 (April 28, 2025)
* Fix docker crash where journal not configured correctly.
* Updates to config editor.
* Increased number of virtual buttons. (previous limitation only effected RS 16 panels).
* Changed to dimmable lights.

---

## Release 2.6.3 (April 13, 2025)
* AqualinkD can now self-update directly from github. Use `Upgrade AqualinkD` button in Aqmanager
* New install and remote_install scripts.
* Changed MQTT posting frequency when Timers are enabled for better performance.
* Updates for new repo location. [AqualinkD organization](https://github.com/aqualinkd/AqualinkD)
* Upgrade command:
  * `curl -fsSL https://install.aqualinkd.com | sudo bash -s -- latest` 
  * -or- 
  * `curl -fsSL https://raw.githubusercontent.com/aqualinkd/AqualinkD/master/release/remote_install.sh | sudo bash -s -- latest`

---

## Release 2.6.1 (March 26, 2025)
* Added External Sensors to Web UI & HomeKit.
* Added Heat Pump / Chiller Thermostat to Web UI & HomeKit.
* Fixed some bugs in Configuration Editor.
* Link device/virtual/onetouch button with SWG BOOST. (Allows you to set VSP RPM when in Boost mode)

---

## Release 2.6.0 (March 22, 2025)
* Added configuration editor in aqmanager. [Wiki - AQManager](https://github.com/aqualinkd/AqualinkD/wiki#AQManager)
* Can now self-configure on startup. Set `device_id=0xFF`
* Added scheduling of pump after events (Power On, Freeze Protect, Boost)
* Fixed HA bug for thermostats not converting to °C when HA is set to display °C.
* Added support for monitoring SBC system sensors, like CPU / GPU / Board (CPU temp being most useful).
* Reduced load on panel over AqualinkTouch protocol.
* Fixed higher than normal CPU load when leaving aqmanager open and sending no messages (leaving aqmanager open for over 14 days).
* Reworked PDA sleep mode.
* Added support for Heat Pump / Chiller support.

---

## Release 2.5.1 (December 7, 2024)
* Patch release

---

## Release 2.5.0 (November 16, 2024)
* PDA panel Rev 6.0 or newer that do not have a Jandy iAqualink device attached can use the AqualinkTouch protocol rather than PDA protocol.
  * This is faster, more reliable and does not interfere with the physical PDA device (like existing implementation) 
  * Please consider this very much BETA at the moment.
  * Use `device_id=0x33` in aqualinkd.conf.
* PDA panel Rev 6.0 of newer WITH a Jandy iAqualink device attached can use `read_RS485_iAqualink = yes` to speed up device state change detection.
* Added MQTT vsp_pump/speed/set for setting speed (RPM/GPM) by %, for automation hubs.
* Added full dimmer range support for dimmable lights (not limited to 0,25,50,75,100 anymore)
* Added vsp and dimmer to Hassio and homebridge-aqualinkd plugin as full range dimmer controls.
* Added color lights & dimmer to Hassio as selectors.
* Updated UI for support fullrange dimmer.
* Cleaned up code for spa_mode and spa for newer panels.
* Allow VSP to be assigned to virtual button.
* Fixed bug with timer not starting.
* Increase Speed of detecting device state changes.
* Added iAqualink2 protocol support.
* Changed color light logic.
* Faster OneTouch device support.

---

## Release 2.4.0 (September 7, 2024)

**⚠️ WARNING: Breaking change** - If you use dimmer, please change `button_??_lightMode` from 6 to 10.

* Fixed bugs with particular Jandy panel versions and color lights.
* Added support for more color lights, and sped up programming
* Code & Repo refactor
* Decoded more Pentair VSP pump status.
* Changed VSP pump status handling (display more in web UI).
* VSP Pump status & other attributes in HASSIO.
* Dual temperature sensors supported.
* Updates to serial_logger.
* Changes to aqmanager for adding more options for decoding protocols.
* Support for packets changes from panels REV Yg
* Support VSP by label (not pump number), REV Yg
* Added support for One Touch Buttons & Virtual Buttons.
  * Look at `virtual_button??_label` in aqualinkd.conf
  * Have to use AqualinkTouch protocol for `extended_device_id` 0x31->0x33 in aqualinkd.conf

---

## Release 2.3.8 (September 3, 2024)
**Released and immediately removed due to bug with light program 0.**

---

## Release 2.3.7 (June 11, 2024)
* Fix for Pentair VSP losing connection & bouncing SWG to 0 and back.
* Added more VSP data (Mode, Status, Pressure Curve, both RPM & GPM) for all Pentair Pumps (VS/VF/VSF).
* Updates to HomeAssistant integration.
  * Will now convert from C to F so setting `convert_mqtt_temp_to_c` doesn't affect Hassio anymore
  * Added VSP support to change RPM/GPM (uses fan type since Hassio doesn't support RPM, so it's a % setting of the full RPM or GPM range of your pump) 
* Updates to serial_logger.
* UI improvements
  * Will display both RPM & GPM for VSP (space providing)
  * Fix freeze protect button in UI not showing enabled.
* Updates to AQmanager UI.

---

## Release 2.3.6 (May 31, 2024)
* No functionality changes
* Build & Docker changes
* Going forward AqualinkD will release binaries for both armhf & arm64
  * armhf = any Pi (or equiv board) running 32 bit Debian based OS, stretch or newer
  * arm64 = Pi3/4/2w running 64 bit Debian based OS, buster or newer

---

## Release 2.3.5 (May 23, 2024)
* Added Home Assistant integration through MQTT discover
  * Please read the Home Assistant section of the [Wiki - HASSIO](https://github.com/aqualinkd/AqualinkD/wiki#HASSIO)
  * There are still some enhancements to come on this.
* Included Docker into main releases
  * Please read Docker section of the [Wiki - Docker](https://github.com/aqualinkd/AqualinkD/wiki#Docker)
* Added support for reading extended information for Jandy JXi heaters.
* Added Color Light to iAqualinkTouch protocol.
* Fixed issue with mqtt_timed_update (1~2 min rather than between 2 & 20 min)

---

## Release 2.3.4 (May 13, 2024)
* Changes for Docker
* Updated simulator code base and added new simulators for AllButton, OneTouch & PDA.
  * `<aqualinkd.ip>/allbutton_sim.html`
  * `<aqualinkd.ip>/onetouch_sim.html`
  * `<aqualinkd.ip>/aquapda_sim.html`
    * On PDA only panel AqualinkD has to share the same ID with the PDA simulator. Therefore AqualinkD will not respond to commands while simulator is active.
* Now you can completely program the control panel with the simulators removing the need to have a Jandy device.

---

## Release 2.3.3 (May 30, 2024)
* Introduced Aqualink Manager UI `http://aqualink.ip/aqmanager.html`
  * [AqualinkD Manager](https://github.com/aqualinkd/AqualinkD/wiki#AQManager)
* Moved logging into systemd/journal (journalctl) from syslog
  * [AqualinkD Log](https://github.com/aqualinkd/AqualinkD/wiki#Log)
* Updated scheduler
  * [AqualinkD Scheduler](https://github.com/aqualinkd/AqualinkD/wiki#Scheduler)
* Introduced RS485 frame delay / timer. 
  * Improve PDA panels reliability (PDA panels are slower than RS panels)
  * Potentially fixed Pentair VSP / SWG problems since Pentair VSP uses a different protocol, this will allow a timed delay for the VSP to post a status messages. Seems to only affect RS485 bus when both a Pentair VSP and Jandy SWG are present.
  * Add `rs485_frame_delay = 4` to /etc/aqualinkd.conf, 4 is number of milliseconds between frames, 0 will turn off ie no pause.
* PDA Changes to support SWG and Boost.

---

## Release 2.3.2 (July 8, 2023)
* Added support for VSP on panel versions REV 0.1 & 0.2
* Can change heater slider min/max values in web UI. `./web/config.js`
* Added reading ePump RPM/Watts directly from RS485 bus.

---

## Release 2.3.1 (June 23, 2023)
* Changed a lot of logic around different protocols.
* Added low latency support for FTDI usb driver.
* AqualinkD will find out the fastest way to change something depending on the protocols available.
* Added scheduler (click time in web ui). Supports full calendar year (ie seasons), See wiki for details. 
* Added timers for devices (ie can turn on Pump for x minutes), Long press on device in WebUI.
* Timers supported in MQTT/API.
* Support for external chem feeders can post to AqualinkD (pH & ORP shown in webUI / homekit etc)
* Add support for dimmers.
* Extended SWG status now in web UI.
* Serial logging / error checking enhancements.
* Added simulator back. (+ Improved UI).
* Fix issue with incorrect device state after duplicate MQTT messages being sent in rapid succession (< 0.5 second).
* Found a workaround for panel firmware bug in iAqualink Touch protocol where VSP updates sometimes got lost.
* Fix bug in IntelliBrite color lights
* Install script checks for cron and its config (needed for scheduler)
* serial-logger will now give recommended values for aqualinkd.conf
* Lock the serial port to stop duplicate process conflict.
* Lots of code cleanup & modifying ready for AqualinkD Management console in a future release.

---

## Release 2.2.2
* Fixed some Web UI bugs
* Color lights now quicker when selecting existing color mode.

---

## Release 2.2.1
* Supports serial adapter protocol `rssa_device_id` (provides instant heater setpoint changes & setpoint increment)
* Can use separate threads for network & RS485 interface. (optimization for busy RS485 bus implementations)
* Display messages now posted to MQTT
* Finalized all pre-release work in 2.2.0(a,b,c)
* Optimized the USB2RS485 Serial adapter logic.
* Logging bitmasks to focus debugging information
* Serial Logger changes to test for speed / errors & busy network bus
* Changed raw RS485 device reading, to specific devices rather than all
  * Change `read_all_devices` & `read_pentair_packets` to `read_RS485_swg`, `read_RS485_ePump`, `read_RS485_vsfPump`
  * Since you can now target specific device, we've reverted back to displaying the real information of the device & not trying to hide it like the panel does. Please see wiki config section for more details
* Can link Spa Mode and Spa Heater (web UI only). Add `var link_spa_and_spa_heater = true;` to config.js

---

## Pre-Release 2.2.0c
* Cleaned up Makefile (and adding debug timings).
* Changed logging infrastructure.
* Added experimental options for Pi4.
* **2.2.0a (had some issues with compiler optimization) - please don't use or compile yourself.**
* Fixed RS-4 panel bug.
* Fixed some AqualinkTouch programming issues.
* Increased timeout for startup probe.

**⚠️ This release REQUIRES you to make aqualinkd.conf changes.** Read wiki section: https://github.com/aqualinkd/AqualinkD/wiki#Version_2

* Extensive work to reduce CPU cycles and unnecessary logic.
* iAqualink Touch protocol supported for VSP & extended programming.
  * This protocol is a lot faster for programming, ID's are between 0x38 & 0x3B `extended_device_id`, use Serial_logger to find valid ID.
  * If your panel supports this it's strongly recommended to use it and also set `extended_device_id_programming=yes`
* New panel config procedure VERY dependent on panel type, you must change config, must set `panel_type` in config.
  * Buttons (specifically) heaters will be different from previous versions. (all info in wiki link above) 
* Simulator support removed for the moment.
* Few changes in RS protocol and timings.
* Fixed bug with Watts display for VSP.
* Fixed bug with colored lights.
* RS16 panels no longer require `extended_device_id` to be set
* More compile flags. See notes in wiki on compiling.
* Extensive SWG logic changes.
* AqualinkD startup changed to fix some 'systemctl restart' issues.
* More detailed API replies.

---

## Release 2.1.0

**Big update, lots of core changes.** Please read wiki section: https://github.com/aqualinkd/AqualinkD/wiki#Version_2

* Full Variable Speed Pump support. (Can read, set & change RPM, GPM)
* Full support for all Colored Lights (even if Jandy Control Panel doesn't support them)
* ChemLink pH & ORP now supported. (along with posting MQTT information)
* Configuration changes. Make sure to read wiki.
* RS12 & RS16 Panels now supported. (RS16 will also need `extended_device_id` set for full AUXB5-B8 support)
* New UI option(s) `turn_on_sensortiles = true` & `show_vsp_gpm=false` in `config.js`
* Added compile flags. If you compile AqualinkD and have no need for PDA or RS16 support, edit the Makefile.
* Completely new API.

---

## Release 1.3.9a
* Improved Debugging for serial.
* Added panel Timeout mode support to UI and MQTT
* Fixed SWG bug while in Service & Timeout modes
* Cleaned up SWG code and MQTT status messages for SWG and SWG/enabled
* Fixed SWG bounce when changing SWG %

---

## Release 1.3.8
* Fixed PDA mode from 1.3.7
* Added SWG Boost to PDA
* More updates to protocol code for Jandy and Pentair.

---

## Release 1.3.7

**⚠️ PDA SUPPORT IS BROKEN IN 1.3.7 - DO NOT UPGRADE IF YOU'RE USING PDA MODE**

**Note:** Due to changes to speed up programming the control panel, PDA mode does not function correctly. Will be fixed in a future release.

* SWG updates
* Simulator update
* Added boost functionality for SWG. (Web UI & MQTT only, not Apple homekit yet)
    * MQTT boost status is `aqualinkd/SWG/Boost`
    * MQTT boost on/off is `aqualinkd/SWG/Boost/set`
    * Web UI: long press on SWG icon for boost & percent options
    * Simple Web UI: extra button called Boost
* Changed how programming works. (Please test fully things like, changing heater setpoints, SWG percent etc, be prepared to rollback)
* Added raw RS485 logging

---

## Release 1.3.6
* Can now debug inline from a web ui. (`http://aqualinkd.ip.address/debug.html`)
* Fix SWG in homekit sometimes displaying wrong value. 
  * **Note to Homekit users:** Upgrading to 1.3.5c (and above) will add an additional SWG PPM tile (look in default room). You'll need to update homebridge-aqualinkd to 0.0.8 (or later) to remove the old PPM tile (or delete your homebridge cache). This is due to a bug in homebridge-aqualinkd < 0.0.7 that didn't delete unused tiles.
* Logic for SWG RS485 checksum_errors.
* Fixed pentair packet logging, missing last byte.
* Support for two programmable lights. (Note must update your aqualinkd.conf).
* Can now display warnings and errors in the web UI (as well as log).
* Fix memory issue with PDA.
* Better support for "single device mode" on PDA.
* Fix memory leak in web UI with some browsers.
* Changes for better portability when compiling on other systems.

---

## Release 1.3.5
* Fixed SWG bug showing off/0% every ~15 seconds (introduced in 1.3.3).
* PDA updates for freeze protect/SWG and general speed increase.

---

## Release 1.3.4 (a)
* Logging changes.
* Fix issues in programming mode.
* Update to simulation mode.
* Changed to serial logger.
* PDA changes for SWG and Setpoints.

---

## Release 1.3.3 (a, b, c, e, f)
* Incremental PDA fixes/enhancements.
* SWG bug fix.

---

## Release 1.3.3
* AqualinkD will now automatically find a usable ID if not specifically configured.
* Support for reading (up to 4) Variable Speed Pump info and assigning per device. (Please see wiki for new config options).
  * **Note:** At present only Pentair VSP is supported. If you have Jandy VSP (ePump) and are willing to do some testing, please post on forum as we'd like to get this supported as well.
  * For VSP you will need to check configuration for `read_all_devices = yes` & `read_pentair_packets = yes` and assign RS485 Pump ID to Device ID in configuration. Serial_logger should find ID's for you.
  * WebUI will display Pump RPM only. RPM, Watts and GPH information is also available from MQTT & API.

---

## Release 1.3.2c
* Miscellaneous bug fixes and buffer overrun (could cause core dump).
* VSP update & Pentair device support.

---

## Release 1.3.1
* Changed the way PDA mode will sleep.
* Added preliminary support for Variable Speed Pumps. (Many limitations on support).
* Added int status to Web API.

---

## Release 1.3.0

**Large update for PDA only control panels** (Majority of this is ballle98 work)

* Can distinguish between AquaPalm and PDA supported control panels.
* PDA Freeze & Heater setpoints are now supported.
* Added PDA Sleep mode so AqualinkD can work in conjunction with a real Jandy PDA.
* Sped up many PDA functions.
* Fixed many PDA bugs.

**Non-PDA specific updates:**
* Can get button labels from control panel (not in PDA mode).
* RS485 Logging so users can submit information on Variable Speed Pumps & other devices for future support.
* Force SWG status on startup, rather than wait for pump to turn on.
* General bug fixes and improved code in many areas.

---

## Release 1.2.6f
* Solution to overcome bug in Mosquitto 1.6.
* Fixed Salt Water Generator when % was set to 0.
* Added support for different SWG % for pool & spa. (SWG reports and sets the mode that is currently active).
* Increased speed of SWG messages.
* Few other bug fixes (Thanks to ballle98).

---

## Release 1.2.6e
**Unstable update.** Please only use if you need one of the items below.

---

## Release 1.2.6c
* Fixed some merge issues.
* Added MQTT topic for delayed start on buttons.
* Removed MQTT flash option for delayed start (never worked well anyway).

---

## Release 1.2.6b
* Added MQTT topic for full SWG status (MQTT section in see wiki).
* Configured option to turn on/off listening to extended device information.
* Added service mode topic to MQTT (Thanks to tcm0116).
* Added report zero pool temp (Thanks to tcm0116).

---

## Release 1.2.6a
* More PDA fixes (Thanks to ballle98).
* Fix in MQTT requests to change temperature when temperature units are unknown.

---

## Release 1.2.6
* Fix for PDA with SPA messages. (Thanks to ballle98).
* Added report 0 for pool temperature when not available. (Thanks to tcm0116).

---

## Release 1.2.5a
* Fix bug for MQTT freeze protect.

---

## Release 1.2.4
* Small fix for Freeze Protect.

---

## Release 1.2.3
* Fix for setpoints on "Pool Only" configurations.

---

## Release 1.2.2
* Support for Spa OR Pool ONLY mode with setpoints (previous setpoints expected Spa & Pool mode)
* Added support for MQTT Last Will Message.

**Note:** Fixed spelling errors will effect your configuration and the install.sh script will not overwrite.
* Please compare `/var/www/aqualinkd/config.js` to the new one, you will need to manually edit or override.
* MQTT spelling for "enabled" is now accurate, so anything using the `/enabled` message will need to be changed.
* Homekit will also need to be changed. Please see the new homekit2mqtt.json or modify your existing one.

---

## Release 1.2
* PDA support in BETA. (Please see Wiki for details).
* Fixed bug in posting Heater enables topics to MQTT. (order was reversed).
* Serial read change. (Detect escaped DTX in packet, 1 in 10000 chance of happening).

---

## Release 1.1
* Changed the way AqualinkD reads USB, fixes the checksum & serial read too small errors that happened on some RS485 networks.
* Fixed bug in SWG would read "high voltage" and not "check cell".

---

## Release 1.0e
* Web UI out of Beta.
* MQTT fix setpoints.
* Simulator is now more stable.
* Updates to serial logger.
* UI updates.
* Bug fix in MQTT_flash (still not perfect).

---

## Release 1.0c
* New Simpler interface.
* Start of a RS8 Simulator:
    * You can now program the AqualinkRS from a web interface and not just the control panel.
    * Please make sure all other browsers and tabs are not using AqualinkD as it does not support multiple devices when in simulator mode.
* Fixed a few bugs.

---

## Release 1.0b

**NEW WEB UI!!!** (in beta)

* Flash buttons on/off in homekit for enabling/disabling/cooldown period as they do on the control panel.
* Full SWG support (setting %, not just reporting current state). Also reports Salt Cell status such as (no flow, low salt, high current, clean cell, low voltage, water temp low, check PCB).
* Update to thermostats, colors are now correct in homekit:
  * Green = enabled
  * Orange = heating
  * Blue = cooling (SWG only)
* Light show program mode should now support most vendor lights.
* Configuration changes for: Spa temp as pool temp/light program mode options/enable homekit button flash.
* Updated to serial_logger.
* Freeze protect, heater temperature and SWG set-points have been added to support for standard HTTP requests (MQTT & WS always had support).

---

## Supported Panel Versions

AqualinkD was designed for Jandy Aqualink RS and works with AqualinkRS and iAqualink Combo control panels. It will work with Aqualink PDA/AquaPalm; but with limitations.

**AqualinkD is known to work with Panel Versions from Rev H to the Latest Rev Yg**

---

**For installation & upgrade instructions, please see:** https://github.com/aqualinkd/AqualinkD/wiki
