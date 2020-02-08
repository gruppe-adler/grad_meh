/*
 * Author: DerZade
 * Create loading item control with the given parameters
 *
 * Arguments:
 * 0: Display <DISPLAY>
 * 1: Existing controls group to create item in <CONTROL>
 * 2: worldName <STRING>
 *
 * Return Value:
 * [CONTROL, MAP_DONE]
 *
 * Example:
 * [_display, _grp, "stratis"] call grad_meh_fnc_loadingItem_create;
 *
 * Public: No
 */
#include "../idcmacros.hpp"

#define DONE_TEXTURE "\a3\3den\data\controls\ctrlcheckbox\texturechecked_ca.paa"

params ["_display", "_parentGrp", "_worldName"];

private _STEPS = [
	["read_wrp", IDC_LOADINGITEM_STEP_READWRP],
	["write_sat", IDC_LOADINGITEM_STEP_SATIMAGE],
	["write_houses", IDC_LOADINGITEM_STEP_HOUSES],
	["write_preview", IDC_LOADINGITEM_STEP_PREVIEWIMGE],
	["write_meta", IDC_LOADINGITEM_STEP_META],
	["write_dem", IDC_LOADINGITEM_STEP_DEM]
];

private _displayName = [(configFile >> "CfgWorlds" >> _worldName), "description", ""] call (uiNamespace getVariable "BIS_fnc_returnConfigEntry");

private _error = [(uiNamespace getVariable ["grad_meh_errors", []]), _worldName, ""] call (uiNamespace getVariable "BIS_fnc_getFromPairs");

if !(_error isEqualTo "") exitWith {
	private _item = _display ctrlCreate ['grad_meh_loadingItem_error', -1, _parentGrp];

	// set name text and color to purple
	private _nameCtrl = (_item controlsGroupCtrl IDC_LOADINGITEM_NAME);
	if !(isNull _nameCtrl) then {

		_nameCtrl ctrlSetText _displayName;
		_nameCtrl ctrlSetTextColor [0.56, 0.067, 0.404, 1];
	};

	// set error text
	private _errorCtrl = (_item controlsGroupCtrl IDC_LOADINGITEM_ERROR);
	if !(isNull _errorCtrl) then {
		_errorCtrl ctrlSetText _error;
	};

	[_item, false];
};

private _item = _display ctrlCreate ['grad_meh_loadingItem', -1, _parentGrp];

(_item controlsGroupCtrl IDC_LOADINGITEM_NAME) ctrlSetText _displayName;

private _runningAnim = {
	params ["_stepCtrl"];

	private _box = _stepCtrl controlsGroupCtrl IDC_LOADINGSTEP_PICTURE;

	private _dir = 180;
	while {!(_box isEqualTo controlNull)} do {
		if (_box isEqualTo controlNull) exitWith {};
		_box ctrlSetAngle [_dir, 0.5, 0.5, false];
		_box ctrlCommit 0.7;
		_dir = _dir + 180;
		waitUntil { ctrlCommitted _box };
	};
};


private _allDone = true;
// update states
{
	_x params ["_step", ["_idc", -1]];

	private _status = [_worldName, _step] call (uiNamespace getVariable "grad_meh_fnc_stepStatus");
	private _stepCtrl = _item controlsGroupCtrl _idc;

	if !(_stepCtrl isEqualTo controlNull) then {
		if (_status isEqualTo "done") then {
			(_stepCtrl controlsGroupCtrl IDC_LOADINGSTEP_PICTURE) ctrlSetText DONE_TEXTURE;
			(_stepCtrl controlsGroupCtrl IDC_LOADINGSTEP_TEXT) ctrlSetTextColor [0.4, 0.667, 0.4, 1];
		};
		if (_status isEqualTo "running") then {
			[_stepCtrl] spawn _runningAnim;
		};
		if (_status isEqualTo "canceled") then {
			(_stepCtrl controlsGroupCtrl IDC_LOADINGSTEP_PICTURE) ctrlSetText DONE_TEXTURE;
			(_stepCtrl controlsGroupCtrl IDC_LOADINGSTEP_PICTURE) ctrlSetTextColor [1, 1, 1, 0.5];
			(_stepCtrl controlsGroupCtrl IDC_LOADINGSTEP_TEXT) ctrlSetTextColor [1, 1, 1, 0.5];
		};
		if !(_status in ["canceled", "done"]) then {
			_allDone = false;
		};
	};
} forEach _STEPS;

if (_allDone) then {
	// set name color to green
	private _nameCtrl = (_item controlsGroupCtrl IDC_LOADINGITEM_NAME);
	if !(_nameCtrl isEqualTo controlNull) then {
		_nameCtrl ctrlSetTextColor [0.4, 0.667, 0.4, 1];
	};
};

[_item, _allDone];
