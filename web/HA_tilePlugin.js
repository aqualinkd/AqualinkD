

/*
 For manual setup, follow the below.
 For easy setup, read the online documentation

 1) put below in aqualinkd config.json
 
 "external_script": "/HA_tilePlugin.js"

 2) Configure any custom icons in aqualinkd config.json, example below 'light.back_floodlights' is HA ID:-

 "light.back_floodlights": {
      "display": "true",
      "on_icon": "extra/light-on.png",
      "off_icon": "extra/light-off-grey.png"
    },

  3) Add the HA ID's you want to the setTile function below :-
  homeassistantAction("light.back_floodlights");

  4) IN HOME ASSISTANT you will need to allow Cross-Origin Resource Sharing 
     edit configuration.yaml, and add below (change aqualinkd to the machine name running aqualinkd)

  http:
    cors_allowed_origins
      - http://aqualinkd

  5) Add your HA API token & server below.
*/

// Add your HA API token
var HA_TOKEN = '<YOUR LONG HA TOKEN HERE>';
// Change to your HA server IP
var HA_SERVER = 'http://192.168.1.255:8123';  


setupTiles();


function setupTiles() {
  // If we have specific user agents setup, make sure one matches.
  if (_config?.HA_tilePlugin && _config?.HA_tilePlugin?.userAgents) {
    var found=false;
    for (const agent of _config.HA_tilePlugin.userAgents) {
      if (navigator.userAgent.search(agent) != -1) {
        found = true;
      }
    }
    // User agent doesn't match the list, return and tdo nothing.
    if (!found) {
      return;
    }
  }

  // If aqualinkd has not added tiles, wait.
  if ( document.getElementById("Filter_Pump") === null) {
    setTimeout(setupTiles, 100);
    return;
  }

  if (!_config?.HA_tilePlugin) {
    // If you are not using config.json to add them, Add your HA ID's below. replace the below examples.
    /* ###########################################################
       #
       #   Add manual entries here
       #
       ########################################################### */
    //homeassistantAction("light.back_floodlights");
  } else {
    HA_TOKEN = _config.HA_tilePlugin.HA_token;
    HA_SERVER = _config.HA_tilePlugin.HA_server;
    for (const id of _config.HA_tilePlugin.HA_entity_ids) {
      homeassistantAction(id);
    }
  }
}

/*. Some helper / formating functions */
function getStringAfterCharacter(mainString, character) {
  const index = mainString.indexOf(character);
  if (index === -1) {
    // Character not found, return the original string or an empty string as desired
    return mainString; 
  }
  return mainString.slice(index + 1);
}
function getStringBeforeCharacter(mainString, character) {
  const index = mainString.indexOf(character);
  if (index === -1) {
    // Character not found, return the original string or an empty string as desired
    return mainString; 
  }
  return mainString.slice(0, index);
}

function formatTwoDecimalsOrInteger(num) {
  const roundedNum = Math.round(num * 100) / 100;
  let result = String(roundedNum);

  // Check if the string ends with '.00' and remove it if present
  if (result.endsWith('.00')) {
    result = result.slice(0, -3); // Remove the last 3 characters (".00")
  }
    
  return result;
}


function HA_switchTileState(id) {
  state = (document.getElementById(id).getAttribute('status') == 'off') ? 'on' : 'off';

  homeassistantAction(id, state);

  setTileOn(id, state);
}


function HA_updateDevice(data) {
  var tile
  var name

  if (!data || !data.state) {
    console.log("Error, missing JSON values ID="+data?.entity_id+", Name="+data?.attributes?.friendly_name+", State="+data?.state);
    return;
  } else if (!data.attributes.friendly_name) {
    name = getStringAfterCharacter(data.entity_id, ".");
  } else {
    name = data.attributes.friendly_name;
  }

  const service = getStringBeforeCharacter(data.entity_id, ".");
  
  // If the tile doesn't exist, create it.
  if ((tile = document.getElementById(data.entity_id)) == null) {
    var tile = {};
    tile["id"] = data.entity_id;
    tile["name"] = name;
    tile["display"] = "true";
    
    if ( service == "sensor" ) {
      tile["type"] = "value";
      tile["value"] = data.state;
    } else {
      tile["type"] = "switch"; // switch or value
    }

    if ( service == "cover") {
      tile["state"] = ((data.state == 'closed') ? 'off' : 'on')
    } else {
      tile["state"] = ((data.state == 'off') ? 'off' : 'on');
    }
    tile["status"] = tile["state"];

    createTile(tile);

    // Make sure we use out own callback for button press.
    subdiv = document.getElementById(data.entity_id);
    subdiv.setAttribute('onclick', "HA_switchTileState('" + data.entity_id + "')");
    tile = document.getElementById(data.entity_id);
  }

  switch (service) {
    case "sensor":
      setTileValue(data.entity_id, formatTwoDecimalsOrInteger(data.state));
      break;
    case "cover":
      setTileOn(data.entity_id, ((data.state == 'closed') ? 'off' : 'on'), null);
      break;
    case "light":
    case "switch":
    case "input_boolean":
    default:
      setTileOn(data.entity_id, ((data.state == 'off') ? 'off' : 'on'), null);
      break;
  }
  
}




function getURL(service, action) {
  switch (service) {
    case "sensor":
      return "";
      break;
    case "cover":
      return '/api/services/'+service+'/'+(action=="on"?"open":"close")+"_cover";
      break;
    case "light":
    case "switch":
    case "input_boolean":
    default:
      return '/api/services/'+service+'/turn_'+action;
      break;
  }
}

function homeassistantAction(id, action="status") {
  var http = new XMLHttpRequest();
  if (http) {
    http.onreadystatechange = function() {
      if (http.readyState === 4) {
        if (http.status == 200) {
          var data = JSON.parse(http.responseText);
          // Sending action on/off returns blank from HA, so only bother acting on status messages
          if (action == "status") {
            HA_updateDevice(data);
          }
        } else {
          console.error("Http error "+http.status);
        }
      }
    }
  };

  if (action == "status") {
    http.open('GET', HA_SERVER+'/api/states/' + id, true);
    http.setRequestHeader("Content-Type", "application/json");
    http.setRequestHeader("Authorization", "Bearer "+HA_TOKEN);
    _poller = setTimeout(function() { homeassistantAction(id, action); }, 5000);
    http.send(null);
  } else {
    const service = getStringBeforeCharacter(id, ".");
    // Below should be api/services/cover/open_cover for cover.
    http.open('POST', HA_SERVER+getURL(service,action), true);
    http.setRequestHeader("Content-Type", "application/json");
    http.setRequestHeader("Authorization", "Bearer "+HA_TOKEN);
    http.send('{"entity_id": "'+id+'"}');
  }

}

