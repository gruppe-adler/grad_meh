/*
 * Author: DerZade
 * Get state of given step of given world. Will return one of the following "done", "running", "pending" or "canceled"
 *
 * Arguments:
 * 0: worldName <STRING>
 * 1: step id <STRING>
 *
 * Return Value:
 * status <STRING>
 *
 * Example:
 * ["Altis", "writeSatImages"] call grad_meh_fnc_stepStatus;
 *
 * Public: No
 */

params ["_worldName", "_step"];

private _progress = uiNamespace getVariable ["grad_meh_progress", []];

// map progress: Array including two arrays:
// index 0 -> includes all steps marked as running
// index 1 -> includes all steps marked as done
// index 2 -> includes all steps marked as canceled
private _mapProgress = [_progress, _worldName, [[], [], []]] call (uiNamespace getVariable "BIS_fnc_getFromPairs");

_mapProgress params ["_running", "_done", "_canceled"];

if (_step in _running) exitWith {
	"running";
};

if (_step in _done) exitWith {
	"done";
};

if (_step in _canceled) exitWith {
	"canceled";
};

"pending";