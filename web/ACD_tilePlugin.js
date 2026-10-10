
/*

For multiple external scripts.

"external_scripts": [
    "/HA_tilePlugin.js",
    "/ACD_tilePlugin.js"
],

for one.

"external_script": "/ACD_tilePlugin.js",
  "ACD_tilePlugin": {
    "ACD_server": "http://aqualinkd-cm:88",
    "ACD_display_temp_in_f": true,
    "ACD_entity_ids": [
      "AquachemD",
      "CS_3",
      "TEMP_1",
      "PH_1",
      "ORP_1",
      "PMP_1",
      "PMP_2"
    ]
  },

*/

// Inject CSS we need
(function () {
  const css = `
  .acd_option_radiocontainer {
    display: flex;
    width: 100%;
    background-color: var(--options_slider_lowlight);
    border-radius: 8px;
    padding: 3px;
    box-sizing: border-box;
     
  }
  .acd_radio-segment {
    flex: 1;
    text-align: center;
  }

  .acd_radio-segment input[type="radio"] {
    display: none;
  }

  .acd_radio-segment .segment-label {
    display: block;
    padding: 2px 0;
    cursor: pointer;
    border-radius: 6px;
    font-size: 0.9rem;
    font-weight: 600;
    color: var(--body_text);
    transition: background-color 0.2s, color 0.2s;
  }

  .acd_radio-segment input[type="radio"]:checked+.segment-label {
    background-color: var(--options_slider_highlight);
    color: #ffffff;
    box-shadow: 0 2px 4px rgba(0, 0, 0, 0.2);
  }
  `;

  const styleElement = document.createElement('style');
  styleElement.type = 'text/css';

  // Inject the CSS string into the style element
  if (styleElement.styleSheet) {
    styleElement.styleSheet.cssText = css; // Support for older IE versions
  } else {
    styleElement.appendChild(document.createTextNode(css)); // Modern browsers
  }

  // Append the style element to the document head
  document.head.appendChild(styleElement);

  // Create some default icons
  if (_config?.["colors" + _theme]?.tile_on_background) {
    _sensorOnUri = createSensorIcon(_config["colors" + _theme].tile_on_background, false);
  } else {
    _sensorOnUri = createSensorIcon("rgb(255, 255, 255)", false);
  }

  if (_config?.["colors" + _theme]?.tile_icon_background_color_disabled) {
    _sensorOffUri = createSensorIcon(_config["colors" + _theme].tile_icon_background_color_disabled, false);
  } else {
    _sensorOffUri = createSensorIcon("rgb(110, 110, 110)", false);
  }

})();


let _acd_socket_di;
let _acd_devices = {};

acd_setupTiles();

function acd_setupTiles() {

  // If aqualinkd has not added tiles, wait.
  if (document.getElementById("Filter_Pump") === null) {
    setTimeout(acd_setupTiles, 10);
    //console.log("wait");
    return;
  }

  //if (_config?.ACD_tilePlugin && _config.ACD_tilePlugin.ACD_server) {
  if (_config && _config.ACD_tilePlugin && _config.ACD_tilePlugin.ACD_server) {
    acd_start_websocket(_config.ACD_tilePlugin.ACD_server);
  } else {
    /* Not configured to run */
    console.log("ACD_tilePlugin not configured, skipping.");
  }
}

function acd_get_devices() { _acd_socket_di.send(JSON.stringify({ uri: "devices" })); }
function acd_get_dosehistory() { _acd_socket_di.send(JSON.stringify({ uri: "dosestats" })); }

function acd_send_switch_timer(id, value) { _acd_socket_di.send(JSON.stringify({ uri: id + "/timer/set", value: value })); }
function acd_send_switch_cmd(id, value) { _acd_socket_di.send(JSON.stringify({ uri: id + "/set", value: value })); }

function acd_pmp_options_closed(active_option) {

  //console.log("acd_pmp_options_closed");

  const tile_options = document.getElementById('timer_options');
  const id = tile_options.getAttribute('device_id');
  const device = _acd_devices.devices[id];

  // Get current slider value
  //let onTime = document.getElementById('acd_timer_slider_range')?.value ?? 0;
  // aove fails on old ipad.
  const sliderEl = document.getElementById('acd_timer_slider_range');
  let onTime = sliderEl ? sliderEl.value : 0;

  // Get selected radio state
  const selectedRadio = document.querySelector(`input[name="state_toggle_${id}"]:checked`);
  const selectedState = selectedRadio ? parseInt(selectedRadio.value, 10) : device.int_status;

  // Send commands ONLY on close
  if (device.type === "switch") {
    // Always send the state change
    if (selectedState != _acd_devices.devices[id].int_status) {
      if (selectedState === 1 && onTime > 0) {
        acd_send_switch_timer(id, onTime);
      } else {
        acd_send_switch_cmd(id, selectedState);
      }
    } else {
      //console.log("NO CHANGE");
    }
  }

  // Remove all the entries we added for dose history
  let rows = document.querySelectorAll('.dynamic-option-row');
  rows.forEach(row => row.remove());

  // Show the standard on/off toggle
  DisplayTableRow("timer_switch_text_value", true);
  DisplayTableRow("timer_slider_text_value", true);
  DisplayTableRow("timer_slider_range", true);

  showTileOptions(false, id, null);
}

function acd_tile_click(id) {
  console.log("Clicked tile with id: " + id);

  let state = 'unknown';
  if (id == "AquachemD") {
    state = document.getElementById(id).getAttribute('status');
    //send_ACD_command(id, state == 'on' ? 0 : 1);
    acd_send_switch_cmd(id, state == 'on' ? 0 : 1);
  } else if (id.startsWith("PMP_")) {
    document.getElementById(id).setAttribute('type', 'switch_timer');
    showTileOptions(true, id, id);

    // Get History
    document.getElementById('timer_options')?.setAttribute('device_id', id);
    acd_get_dosehistory();

    // Get state
    const oswitch = document.getElementById('timer_switch');
    const state = oswitch?.checked;

    //console.log(id + " state = " + _acd_devices.devices[id]?.int_status);
    switch (_acd_devices.devices[id]?.int_status) {
      case 2: // Enabled.
        oswitch.checked = false;
        break;
    }

    const toggleHtml = buildStateToggle(_acd_devices.devices[id]);
    if (toggleHtml) {
      const header = document.getElementById("timer_option_title")?.closest("tr");
      header.insertAdjacentHTML('afterend', toggleHtml);

      // Hide the standard on/off toggle, sider & text
      DisplayTableRow("timer_switch_text_value", false);
      DisplayTableRow("timer_slider_text_value", false);
      DisplayTableRow("timer_slider_range", false);
    }

    const slider = document.getElementById("acd_timer_slider_range");
    slider.oninput = () => ACD_SyncTileOptions(slider);
    ACD_SyncTileOptions();

    // Add our own handler copy directly from index.html (except our own acd_clickHandler)
    var cTime;
    try {
      cTime = performance.now();
    } catch (e) {
      //cTime = Date.now(); // NSF Probably won't test well below, need to check.
      cTime = 0;
    }

    var acd_clickHandler = function (e) {
      // Short time diff means event was the one that launched the options pane, so ignore
      // Above description is incorrect, we get here when click is OFF the tile
      if ((e.timeStamp - cTime) > 1000) {
        acd_pmp_options_closed(false);
        document.getElementById("body_wrap").removeEventListener("click", acd_clickHandler);
      }
    };

    document.getElementById("body_wrap").addEventListener("click", acd_clickHandler);

    // Overwrite standard mouse clicks.
    let close_button = document.getElementById("timer_options_close");
    //close_button.onclick = acd_pmp_options_closed;
    close_button.onclick = function() {
      document.getElementById("body_wrap").removeEventListener("click", acd_clickHandler);
      acd_pmp_options_closed();
    };
  }
}

function DisplayTableRow(closeID, show) {
  const tr = document.getElementById(closeID)?.closest("tr");

  if (tr && show) {
    tr.style.display = '';
  } else {
    tr.style.display = 'none';
  }
}

function ACD_SyncTileOptions() {
  const tile_options = document.getElementById('timer_options');
  let id = tile_options.getAttribute('device_id');
  const time_slider = document.getElementById('acd_timer_slider_range');
  const time_slider_output = document.getElementById('acd_timer_slider_text_value');

  if (time_slider && time_slider_output) {
    //let tm_ext = useMinutesForRuntime(id) ? ' runtime (minutes)' : ' runtime (seconds)';
    let tm_ext = ' runtime (seconds)';
    time_slider_output.innerHTML = time_slider.value + tm_ext;

    // --- LOCK OUT LOGIC ---
    // Find out which button is currently selected in the UI
    const selectedRadio = document.querySelector(`input[name="state_toggle_${id}"]:checked`);
    //const selectedState = selectedRadio ? parseInt(selectedRadio.value, 10) : _devices.devices[id].int_status;
    // Change this line inside ACD_SyncTileOptions():
    const selectedState = selectedRadio ? parseInt(selectedRadio.value, 10) : _acd_devices.devices[id].int_status;
    // Grab both the text row and the slider row
    const timerRows = document.querySelectorAll('.timer-control-row');

    if (selectedState === 1) { // 1 = ON
      time_slider.disabled = false;
      timerRows.forEach(row => {
        row.style.opacity = '1';
        row.style.pointerEvents = 'auto'; // Re-enable interaction
      });
    } else { // OFF (0) or ENABLED (2)
      time_slider.disabled = true;
      timerRows.forEach(row => {
        row.style.opacity = '0.3'; // Greys out the text and slider
        row.style.pointerEvents = 'none'; // Prevents dragging completely
      });
    }
  }
}

function shortenTimestamp(timestampStr) {
  if (!timestampStr) return "N/A";
  const dateObj = new Date(timestampStr);
  if (isNaN(dateObj.getTime())) return "Invalid Date";
  const options = { month: 'short', day: 'numeric', hour: '2-digit', minute: '2-digit', hour12: false };
  return dateObj.toLocaleDateString('en-US', options).replace(/,/g, '');
}

function buildStateToggle(device) {
  const hasOff = device.attributes?.includes("set_off");
  const hasOn = device.attributes?.includes("set_on");
  const hasEnabled = device.attributes?.includes("set_enabled");

  // Determine if the device is currently disabled
  const isCurrentlyDisabled = (device.int_status === 3); // 3 = DISABLED

  let html = `<tr class='dynamic-option-row'><td colspan='2'><div class="acd_option_radiocontainer">`;

  // Logic: Only show OFF if disabled, otherwise show all available
  if (hasOff) {
    html += `<label class="acd_radio-segment"><input type="radio" name="state_toggle_${device.id}" value="0" ${device.int_status === 0 ? "checked" : ""} onchange="ACD_SyncTileOptions(this);"><span class="segment-label">Off</span></label>`;
  }
/*. May replace above with below
  if (hasOff) {
    // Check if it's 0 OR if the device is currently disabled (3)
    const isChecked = (device.int_status === 0 || device.int_status === 3) ? "checked" : "";
    html += `<label class="acd_radio-segment"><input type="radio" name="state_toggle_${device.id}" value="0" ${isChecked} onchange="ACD_SyncTileOptions(this);"><span class="segment-label">Off</span></label>`;
  }
*/
  if (!isCurrentlyDisabled) {
    if (hasOn) {
      html += `<label class="acd_radio-segment"><input type="radio" name="state_toggle_${device.id}" value="1" ${device.int_status === 1 ? "checked" : ""} onchange="ACD_SyncTileOptions(this);"><span class="segment-label">On</span></label>`;
    }
    if (hasEnabled) {
      html += `<label class="acd_radio-segment"><input type="radio" name="state_toggle_${device.id}" value="2" ${device.int_status === 2 ? "checked" : ""} onchange="ACD_SyncTileOptions(this);"><span class="segment-label">Enable</span></label>`;
    }
  } else {
    // Optional: Add a visual indicator that it's disabled
    html += `<span style="color:red; font-size:12px; margin-left:10px;">(Device Disabled)</span>`;
  }

  html += `</div></td></tr>`;

  html += `
  <tr class="dynamic-option-row timer-control-row" style="opacity: 0.3; pointer-events: none;">
    <td colspan="2" align="center"><span class="option_text" id="acd_timer_slider_text_value">runtime (seconds)</span></td>
  </tr>`;

  html += `
  <tr class='dynamic-option-row timer-control-row'>
    <td colspan='2'>
      <div class="slidecontainer">
        <input type="range" class="option_slider" id="acd_timer_slider_range" value=0 oninput="SyncTileOptions();">
      </div>
    </td>
  </tr>
`;

  return html;
}

function showDoseHistory(data) {
  const tile_option = document.getElementById('timer_options');
  if (tile_option.style.display === 'none' || tile_option.classList.contains("hide")) return;


  const targetDeviceId = tile_option.getAttribute('device_id');
  //console.log("ID = " + targetDeviceId);

  const closeButton = document.getElementById("timer_options_close");
  const footer = closeButton.closest("tr");

  //const footer = document.getElementById('tile_options_table_footer');
  const filteredHistory = data.history.filter(item => item.pump_id === targetDeviceId);

  // Header row
  footer.insertAdjacentHTML('beforebegin', '<tr class="dynamic-option-row"><td colspan="2" align="center"><b>Recent Dosing</b></td></tr>');

  if (filteredHistory.length === 0) {
    footer.insertAdjacentHTML('beforebegin', '<tr class="dynamic-option-row"><td colspan="2" align="center">No history available.</td></tr>');
    return;
  }

  // Create table header
  let tableHtml = `
    <tr class="dynamic-option-row">
      <td colspan="2" style="padding: 5px 0;">
        <table style="width: 100%; font-size: 0.75rem; border-collapse: collapse; text-align: center;">
          <tr style="border-bottom: 1px solid #ccc; font-weight: bold;">
            <td style="padding: 0 2px 0 2px; border-right: 1px solid #ccc;">Time</td>
            <td style="padding: 0 2px 0 2px; border-right: 1px solid #ccc; text-align: right;">Sensor</td>
            <td style="padding: 0 2px 0 2px; border-right: 1px solid #ccc; text-align: right;">Runtime</td>
            <td style="padding: 0 2px 0 2px; text-align: right;">Dose</td>
          </tr>`;

  // Get last 5 items
  const start = Math.max(0, filteredHistory.length - 5);
  for (let i = start; i < filteredHistory.length; i++) {
    const item = filteredHistory[i];
    tableHtml += `
      <tr style="border-bottom: 1px solid #eee;">
        <td style="padding: 0 2px 0 2px; border-right: 1px solid #ccc;">${shortenTimestamp(item.timestamp)}</td>
        <td style="padding: 0 2px 0 2px; border-right: 1px solid #ccc; text-align: right;">${item.reading.toFixed(2)}</td>
        <td style="padding: 0 2px 0 2px; border-right: 1px solid #ccc; text-align: right;">${item.seconds}s</td>
        <td style="padding: 0 2px 0 2px; text-align: right;">${item.ml.toFixed(1)}ml</td>
      </tr>`;
  }

  tableHtml += `</table></td></tr>`;
  footer.insertAdjacentHTML('beforebegin', tableHtml);
}

//function formatDeviceValue(value) {
function formatDeviceValue(value, uom = null) {

  // Convert Celsius to Fahrenheit if configured to do so.
  const uomLower = uom?.toLowerCase();
  if ((uomLower === "°c" || uomLower === "&deg;c") && _config?.ACD_tilePlugin?.ACD_display_temp_in_f) {
    value = (Number(value) * 9 / 5) + 32;
  }

  // Check for int first.
  const str = String(value);
  const parts = str.split('.');
  const integerDigits = parts[0].length;
  let fractionalDigits = parts.length > 1 ? parts[1].length : 0;
  if (fractionalDigits === 0) return value;


  let total_digits = 3;
  if (integerDigits + fractionalDigits <= total_digits) return value;
  else if (integerDigits >= total_digits) return parts[0].toString();
  else return parts[0].toString() + "." + parts[1].substring(0, total_digits - integerDigits);
}


function acd_deviceSort(a, b) {
  // Create a lowercase version of your config array
  const orderArray = _config.ACD_tilePlugin.ACD_entity_ids.map(id => id.toLowerCase());

  // Look up the lowercase IDs
  let indexA = orderArray.indexOf(a.id.toLowerCase());
  let indexB = orderArray.indexOf(b.id.toLowerCase());

  // Fallback for IDs not found in config (push to bottom)
  if (indexA === -1) indexA = Infinity;
  if (indexB === -1) indexB = Infinity;

  return indexA - indexB;
}

function acd_update_devices(data) {

  const physicalDeviceIds = Object.keys(data.devices);
  physicalDeviceIds.forEach(id => {
    const device = data.devices[id];
    device.status = device?.status?.toLowerCase();
    if (device.attributes?.includes("stats")) {
      const statsId = `${id}_stats`;
      if (!data.devices[statsId]) {
        data.devices[statsId] = {
          id: statsId,
          label: `Avg ${device.label}${device.stats?.duration ? `/${device.stats.duration}` : ''}`,
          status: device.status,
          int_status: device.int_status,
          type: 'average',
          attributes: device.attributes,
          value: device.stats?.avg || 0,
          avg: device.stats?.avg || 0,
          max: device.stats?.max || 0,
          min: device.stats?.min || 0,
          duration: device.stats?.duration || 0,
        };
      }
    }
  });


  _acd_devices = data;
  var deviceArray = Object.values(data.devices);
  deviceArray.sort(acd_deviceSort);
  deviceArray.forEach(device => {
    acd_update_device(device);
  });
  return data;
}

function formatDuration(totalSeconds) {
  if (totalSeconds < 0) totalSeconds = 0;
  totalSeconds = Math.floor(totalSeconds);

  if (totalSeconds >= 3600) {
    // Hours present: round to nearest minute, show HH:MM
    const roundedSeconds = Math.round(totalSeconds / 60) * 60;
    const h = Math.floor(roundedSeconds / 3600);
    const m = Math.floor((roundedSeconds % 3600) / 60);
    return `${h.toString().padStart(2, '0')}:${m.toString().padStart(2, '0')}m`;
  }

  // Under an hour: show MM:SS (exact, no rounding)
  const m = Math.floor(totalSeconds / 60);
  const s = totalSeconds % 60;
  return `${m.toString().padStart(2, '0')}:${s.toString().padStart(2, '0')}s`;
}

function getTileExtraStatusText(id) {
  if (_acd_devices.devices[id]?.attributes?.includes("timer")) {
    if (_acd_devices.devices[id].timer_active == 'ON') {
      return _acd_devices.devices[id].timer_duration ? 'Timer ' + formatDuration(_acd_devices.devices[id].timer_duration) : 'On (timer)';
    }
  } else if (_acd_devices.devices[id]?.attributes?.includes("delay")) {
    if (_acd_devices.devices[id].delay_active == 'ON') {
      return _acd_devices.devices[id].delay_duration ? 'Delay ' + formatDuration(_acd_devices.devices[id].delay_duration) : 'On (delay)';
    }
  } 
  return null;
}

function acd_update_device(device) {
  if (_config.ACD_tilePlugin.ACD_entity_ids.includes(device.id)) {
    device.orig_status = device.status;
    device.status = device.status.toLowerCase();
    device.state = device.status.toLowerCase();
    device.name = device.label;
    device.orig_type = device.type;
    if (device.value) {
      device.orig_value = device.value;
      device.value = formatDeviceValue(device.value, device.uom);
      //device.value = reduct(device.value);
    }

    if (device.status == "delay") {
      device.status = 'flash';
    } else if (device.status == "disabled") {
      device.status = 'off';
    } else if (device.status == "safe") {
      device.status = 'on';
    }

    let uom = null;

    switch (device.type) {
      case "sensor":
      case "average":
      case "level_sensor":
        if (device.id.startsWith("TEMP")) {
          device.type = "temperature";
          uom = '&deg;';
        } else if (device.id.startsWith("ORP")) {
          device.type = "value";
          uom = 'mV';
        } else if (device.id.startsWith("TNK")) {
          device.type = "value";
          uom = '%';
        } else {
          device.type = "value";
        }
        break;
      case "binary_sensor":
        device.type = "switch";
        break;
    }

    if (document.getElementById(device.id) == null) {
      //console.log("Device " + device.id + " not found, creating tile.");
      // Add id to display array.
      if (device?.id && !_devices.includes(device.id)) {
        _devices.push(device.id);
      }

      if (device?.name == "pH") { device.name = "Water Chemistry pH" }
      else if (device?.name == "ORP") { device.name = "Water Chemistry ORP" }
      //console.log(device);
      createTile(device);

      if (device.orig_type == 'average') {
        // Need to make 2 line tile.
        const existingStatus = document.getElementById(device.id + "_status");
        if (existingStatus) {
          const newLineHtml = `<div class="tile_status" id="` + device.id + `_status_line2"></div>`;
          existingStatus.insertAdjacentHTML('beforebegin', newLineHtml);
        }

        const existingName = document.getElementById(device.id + "_name");
        existingName.classList.remove("tile_name");
        existingName.classList.add("tile_name_1line");
      }

      // Make sure we use our own callback for button press.
      let tile = document.getElementById(device.id);
      if (tile) {
        tile.setAttribute('onclick', "acd_tile_click('" + device.id + "')");
        //console.log("Setting onclick for "+device.id);
        if (uom) {
          //console.log("Setting UOM for "+device.id+" to "+uom);
          tile.setAttribute('UOM', uom);
          setTileValue(device.id, device.value);
        }
      } else {
        console.log("Error: Could not find tile for device " + device.id + " after creating it.");
      }
    }
    //return;
    //console.log("Device " + device.id + " found, updating tile. "+device.orig_type);
    switch (device.orig_type) {
      case "switch":
        //setTileOn(device.id, ((device.state == 'off') ? 'off' : 'on'), null);
        if (device.int_status == 3) { // disabled is not a known type for AqualinkD
          setTileOn(device.id, 'off');
          setElementHTML(device.id + '_status', formatSatus(device.orig_status));
        } else {
          setTileOn(device.id, device.status.toLowerCase());
          //setTileOnText(device.id, formatSatus(device.status));
          setTileOnText(device.id, getTileExtraStatusText(device.id) ?? formatSatus(device.status));
        }
        break;
      case "sensor":
      case "level_sensor":
        setTileValue(device.id, device.value);
        if (device.int_status == 3) { // disabled is not a known type for AqualinkD
          setTileOn(device.id, 'off');
          setElementHTML(device.id + '_status', 'Disabled');
          //setTileOnText(device.id, 'Disabled');
        } else if (device.int_status == 2) { // enabled is not a known type for AqualinkD sensor
          setTileOn(device.id, 'enabled');
          setTileOnText(device.id, 'Enabled');
          setTileValue(device.id, device.value);
        } else if (device.int_status == 1) {
          setTileOn(device.id, 'on');
          setTileValue(device.id, device.value);
          // Can't use setTileOnText() since it will overwitte anc ourofrange colors / text
          const statusText = document.getElementById(device.id + "_status");
          if (statusText.innerHTML == '') { // Only set text if empty, ie no warning messages
            statusText.innerHTML = 'Sampling';
          }
        } else {
          setTileOn(device.id, ((device.state == 'off') ? 'off' : 'on'), null);
          setTileValue(device.id, device.value);
        }

        if (device.orig_type == "level_sensor") {
          // Get existing label (incase some range warning)
          //let ext = document.getElementById(`${device?.id}_status`)?.innerHTML ?? null;
          const statusText = document.getElementById(device.id + "_status");
          if (statusText.innerHTML == 'Sampling') {
            setTileOnText(device.id, parseFloat(device.alt_value.value).toFixed(2) + formatUOM(device.alt_value.uom));
          } else {
            setTileOnText(device.id, statusText.innerHTML + " " +parseFloat(device.alt_value.value).toFixed(2) + formatUOM(device.alt_value.uom));
          }
        }
        
        break;
      case "binary_sensor":
        if (device.int_status == 4) { // delay is not a known type for AqualinkD (=flash)
          setTileOn(device.id, 'flash');
          // Add "Delay 00:00s" if we have it.
          let status = getTileExtraStatusText(device.id);
          if (status) {
            const statusText = document.getElementById(device.id + "_status");
            statusText.innerHTML = status;
          }
        } else if (device.int_status == 1) { // safe is not a known type for AqualinkD
          setTileOn(device.id, 'on');
          setTileOnText(device.id, 'Safe');
        } else {
          setTileOn(device.id, ((device.int_status == 1) ? 'on' : 'off'), null);
          setElementHTML(device.id + '_status', formatSatus(device.status));
        }

        const el = document.getElementById(device.id + '_tile_icon_value');
        if (el && device.int_status == 1 || device.int_status == 4) { // overwrite standard SVG icon
          el.style.backgroundImage = _sensorOnUri;
        } else {
          el.style.backgroundImage = _sensorOffUri;
        }
        break;
      case "average":
        setTileValue(device.id, device.value);
        let ext = '';
        if (uom != null) { ext = '<span class="uom_text">' + uom + '</span>'; }
        document.getElementById(device.id + "_status").innerHTML = "Max : " + formatDeviceValue(device.max) + ext;
        document.getElementById(device.id + "_status_line2").innerHTML = "Min : " + formatDeviceValue(device.min) + ext;
        break;
    }

    //setTileOn(device.id, ((device.state == 'off') ? 'off' : 'on'), null);
  }
}

function setTilesUnavailable()
{
  var deviceArray = Object.values(_acd_devices.devices);
  deviceArray.forEach(device => {
    let entity = document.getElementById(device.id);
    if (entity != null) {
      setTileOn(device.id, 'off');
      setElementHTML(device.id + '_status', 'Unavailable');
    }
  });

}

function acd_get_devices() {
  var msg = {
    uri: "devices"
  };
  _acd_socket_di.send(JSON.stringify(msg));
}

function toWebSocketUrl(httpUrl) {
  if (/^wss?:\/\//i.test(httpUrl)) {
    return httpUrl; // already ws:// or wss://, leave as-is
  }
  if (/^https?:\/\//i.test(httpUrl)) {
    return httpUrl.replace(/^http/i, 'ws'); // http:// -> ws://, https:// -> wss://
  }
  return 'ws://' + httpUrl; // no scheme present, assume ws://
}

function acd_start_websocket(server) {
  _acd_socket_di = new WebSocket(toWebSocketUrl(server));
  try {
    _acd_socket_di.onopen = function () {
      // success!
      acd_get_devices();
      // Set recurring fetch every 1 minute
      if (!window.devicesInterval) {
        window.devicesInterval = setInterval(() => { acd_get_devices(); }, 60 * 1000);
      }
    }
    _acd_socket_di.onmessage = function got_packet(msg) {
      document.getElementById("header").classList.remove("error");
      var data = JSON.parse(msg.data);
      if (data.type == 'devices') { _acd_devices = data; acd_update_devices(_acd_devices); }
      else if (data.type == 'dose_history') showDoseHistory(data);
    }
    _acd_socket_di.onclose = function (event) {
      // something went wrong
      /*
      console.log("WebSocket Closed:");
      console.log("Code:   " + event.code);    // Numeric code (e.g., 1000, 1006)
      console.log("Reason: " + event.reason);  // Text explanation from server
      console.log("Clean:  " + event.wasClean); // Boolean: did the TCP handshake close properly?
      */
      setElementHTML("message", '  Connection error!  ');
      document.getElementById("header").classList.add("error");

      setTilesUnavailable();
      // Try to reconnect every 5 seconds.
      setTimeout(function () {
        acd_start_websocket(server)
      }, 5000);
    }
  } catch (exception) {
    alert('<p>Error' + exception);
  }
}

/*
   UI related stuff
*/

function createSensorIcon(color, largeSize) {
  var sensorContent;
  if (!largeSize) {
    /* Standard Size: Centered probe with side pulse lines */
    sensorContent = `<svg xmlns="http://www.w3.org/2000/svg"
                          viewBox="0 0 45 45"
                          stroke="`+ color + `" fill="none"
                          stroke-width="3.5" stroke-linecap="round">
                          <path d="M22.5 10 V30" />
                          <circle cx="22.5" cy="30" fill="`+ color + `" r="3" />
                          <line x1="8" y1="20" x2="16" y2="20" />
                          <line x1="29" y1="20" x2="37" y2="20" />
                        </svg>`;
  } else {
    /* Large Size: Expanded probe body and wider pulse lines */
    sensorContent = `<svg xmlns="http://www.w3.org/2000/svg"
                      viewBox="0 0 45 45"
                      stroke="`+ color + `" fill="none"
                      stroke-width="4.5" stroke-linecap="round">
                      <path d="M22.5 7 V28" />
                      <circle cx="22.5" cy="32" fill="`+ color + `" r="4" />
                      <line x1="5" y1="18" x2="15" y2="18" />
                      <line x1="30" y1="18" x2="40" y2="18" />
                    </svg>`;
  }
  const encodedSensorContent = encodeURIComponent(sensorContent);
  return `url("data:image/svg+xml;charset=utf-8,${encodedSensorContent}")`;
}

/*
if (_config?.["colors" + _theme]?.tile_on_background) {
  _sensorOnUri = createSensorIcon(_config["colors" + _theme].tile_on_background, false);
} else {
  _sensorOnUri = createSensorIcon("rgb(255, 255, 255)", false);
}

if (_config?.["colors" + _theme]?.tile_icon_background_color_disabled) {
  _sensorOffUri = createSensorIcon(_config["colors" + _theme].tile_icon_background_color_disabled, false);
} else {
  _sensorOffUri = createSensorIcon("rgb(110, 110, 110)", false);
}
*/