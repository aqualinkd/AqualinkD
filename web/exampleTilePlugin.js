/*

put below in aqualinkd config.json
 
 "external_script": "/exampleTilePlugin.js"

*/

exampleCreateTile()


function exampleCreateTile() {
  var tile = {};
  tile["id"] = "my.unique.id";
  tile["name"] = "Example Switch";
  tile["display"] = "true";   
  tile["type"] = "switch"; // switch or value
  tile["state"] = 'on';
  tile["status"] = tile["state"];

  createTile(tile);

  // Make sure we use out own callback for button press.
  var subdiv = document.getElementById(tile["id"]);
  subdiv.setAttribute('onclick', "exampleTilePressedCallback('" + tile["id"] + "')");
}

// use this function to update the state or value of a tile
function exampleUpdateTileStatus() {
  // For Switch
  setTileOn("my.unique.id", 'on');

  // For value
  setTileValue("my.unique.id", "0.00");
}

// This will be called when a tile is clicked in the UI.
function exampleTilePressedCallback() {
  // Get the state of the tile
  var state = (document.getElementById(id).getAttribute('status') == 'off') ? 'on' : 'off';
  /*
    Action what needs to happen.  ie send request to home automation hub.
  */
  setTileOn(id, state);
}