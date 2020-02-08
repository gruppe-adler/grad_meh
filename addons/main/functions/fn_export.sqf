/*
 * Author: DerZade
 * Start export of given maps with given params
 *
 * Arguments:
 * 0: Maps to export <ARRAY>
 * 1: Export sat images (Optional, Default: true) <BOOLEAN>
 * 2: Export houses / locations (Optional, Default: true) <BOOLEAN>
 * 3: Export preview image (Optional, Default: true) <BOOLEAN>
 * 4: Export meta.json (Optional, Default: true) <BOOLEAN>
 * 5: Export digital elevation model (Optional, Default: true) <BOOLEAN>
 *
 * Return Value:
 * NONE
 *
 * Example:
 * [["Stratis"]] call grad_meh_fnc_export;
 *
 * Public: No
 */

#include "../status_codes.hpp"

params [
	["_maps", [], []],
	["_exportSat", true, [true]],
	["_exportHouses", true, [true]],
	["_exportPreviewImg", true, [true]],
	["_exportMeta", true, [true]],
	["_exportDem", true, [true]]
];

// reset progess
uiNamespace setVariable ["grad_meh_progress", []];
uiNamespace setVariable ["grad_meh_errors", []];

private _cancelSteps = {
	params ["_step", "_maps"];
	{
		[_x, _step, "canceled"] call (uiNamespace getVariable "grad_meh_fnc_updateProgress");
	} forEach _maps;
};

// set step to canceled if it is turned off
if (!_exportSat) then { ["write_sat", _maps] call _cancelSteps; };
if (!_exportHouses) then { ["write_houses", _maps] call _cancelSteps; };
if (!_exportPreviewImg) then { ["write_preview", _maps] call _cancelSteps; };
if (!_exportMeta) then { ["write_meta", _maps] call _cancelSteps; };
if (!_exportDem) then { ["write_dem", _maps] call _cancelSteps; };

uiNamespace setVariable ["grad_meh_exportOptions", +_this];

disableSerialization;
playScriptedMission [
	'VR',{
		(uiNamespace getVariable ["grad_meh_exportOptions", []]) spawn {
			params [
				["_maps", [], []],
				["_exportSat", true, [true]],
				["_exportHouses", true, [true]],
				["_exportPreviewImg", true, [true]],
				["_exportMeta", true, [true]],
				["_exportDem", true, [true]]
			];


			private _reportError = {
				params [["_world", ""], ["_error", ""]];
				diag_log format ["[GRAD_MEH]: Error while exporting map %1: %2", _world, _error];
				private _errors = uiNamespace getVariable ["grad_meh_errors", []];

				_errors = [_errors, _world, _error, true] call (uiNamespace getVariable "BIS_fnc_setToPairs");

				uiNamespace setVariable ["grad_meh_errors", _errors];

				// redraw loading display
				private _loadingDisplay = uiNamespace getVariable ["grad_meh_loadingDisplay", displayNull];
				if !(isNull _loadingDisplay) then {
					[_loadingDisplay] call (uiNamespace getVariable "grad_meh_fnc_loading_redraw");
				};
			};

			waitUntil { !isNull findDisplay 46 };

			// create loading display
			private _loadingDisplay = (findDisplay 46) createDisplay "grad_meh_loading";
			_loadingDisplay setVariable ["grad_meh_worlds", _maps];
			[_loadingDisplay] call (uiNamespace getVariable "grad_meh_fnc_loading_redraw");

			// export
			{
				private _startedOrAborted = false;

				while { !_startedOrAborted } do {
					private _status = gradMehExportMap [
						_x,
						_exportSat,
						_exportHouses,
						_exportPreviewImg,
						_exportMeta,
						_exportDem
					];

					switch (_status) do {
						case GRAD_MEH_STATUS_OK: {
							_startedOrAborted = true;
						};
						case GRAD_MEH_STATUS_ERR_ARGS: {
							_startedOrAborted = true;
							[_x, "Couldn't export map, because something went wrong when passing arguments."] call _reportError;
						};
						case GRAD_MEH_STATUS_ERR_ALREADY_RUNNING: { /* Just wait and try again next cycle */ };
						case GRAD_MEH_STATUS_ERR_NOT_FOUND: {
							_startedOrAborted = true;
							[_x, "Couldn't export map, because it wasn't found in configFile."] call _reportError;
						};
						case GRAD_MEH_STATUS_ERR_NO_WORLD_SIZE: {
							_startedOrAborted = true;
							[_x, "Couldn't export map, because worldSize is missing in its config."] call _reportError;
						};
						case GRAD_MEH_STATUS_ERR_PBO_NOT_FOUND: {
							_startedOrAborted = true;
							[_x, "Couldn't export map, because the PBO of WRP couldn't be found. (Most likely because it is a EBO)"] call _reportError;
						};
						case GRAD_MEH_STATUS_ERR_PBO_POPULATING: { /* Just wait and try again next cycle */ };
						default {
							_startedOrAborted = true;
							[_x, "An unknown error occurred, while exporting the map."] call _reportError;
						};
					};

					sleep 10;
				};

				diag_log format ["[GRAD_MEH]: Started export map %1 [%2, %3, %4, %5, %6]", _x, _exportSat, _exportHouses, _exportPreviewImg, _exportMeta, _exportDem];
			} forEach _maps;
		};

		// delete temp var from uiNamespace
		uiNamespace setVariable ["grad_meh_exportOptions", nil];

	},
	missionConfigFile,
	true
];

// This is needed for playScriptedMission to work properly
//Close all displays that could be the background display ... this is essentialy forceEnd command
//Closing #0 will cause game to fail
private _zero = findDisplay(0);
{
	if (_x != _zero) then {
		_x closeDisplay 1;
	};
} foreach allDisplays;

failMission "END1";
