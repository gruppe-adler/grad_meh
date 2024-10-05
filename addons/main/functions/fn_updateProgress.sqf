/*
 * Author: DerZade
 * Update progress of loading screen
 *
 * Arguments:
 * 0: worldName <STRING>
 * 1: step id <STRING>
 * 2: status <STRING> either "done", "running", "pending" or "canceled"
 *
 * Return Value:
 * NONE
 *
 * Example:
 * ["Altis", "writeSatImages", "done"] call grad_meh_fnc_updateProgress;
 *
 * Public: No
 */

params ["_worldName", "_step", "_status"];

["grad_meh_progress", _this] call CBA_fnc_localEvent;

private _progress = uiNamespace getVariable ["grad_meh_progress", []];

// map progress: Array including two arrays:
// index 0 -> includes all steps marked as running
// index 1 -> includes all steps marked as done
// index 2 -> includes all steps marked as canceled
private _mapProgress = [_progress, _worldName, [[], [], []]] call (uiNamespace getVariable "BIS_fnc_getFromPairs");

_mapProgress params ["_running", "_done", "_canceled"];

// delete from all arrays
_running deleteAt (_running find _step);
_done deleteAt (_done find _step);
_canceled deleteAt (_done find _step);

// add to respective arrays
if (_status isEqualTo "done") then {
	_done pushBackUnique _step;
};
if (_status isEqualTo "running") then {
	_running pushBackUnique _step;
};
if (_status isEqualTo "canceled") then {
	_canceled pushBackUnique _step;
};

_progress = [_progress, _worldName, [_running, _done, _canceled], true] call (uiNamespace getVariable "BIS_fnc_addToPairs");

uiNamespace setVariable ["grad_meh_progress", _progress];

private _loadingDisplay = uiNamespace getVariable ["grad_meh_loadingDisplay", displayNull];

if !(isNull _loadingDisplay) then {
	[_loadingDisplay] call (uiNamespace getVariable "grad_meh_fnc_loading_redraw");
}
