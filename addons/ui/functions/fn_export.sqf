/*
 * Author: DerZade
 * Start export of given maps with given params
 *
 * Arguments:
 * 0: Parent display for loading display <DISPLAY>
 * 1: Existing controls group to create item in <ARRAY>
 *
 * Return Value:
 * NONE
 *
 * Example:
 * [_display, ["Stratis"], ] call grad_meh_fnc_export;
 *
 * Public: No
 */

params [
	["_parentDisplay", displayNull, [displayNull]],
	["_maps", [], []],
	["_exportSat", true, [true]],
	["_exportHouses", true, [true]],
	["_exportPreviewImg", true, [true]],
	["_exportMeta", true, [true]],
	["_exportDem", true, [true]]
];

// reset progess
uiNamespace setVariable ["grad_meh_progress", []];

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

// create loading display
private _loadingDisplay = _parentDisplay createDisplay "grad_meh_loading";
_loadingDisplay setVariable ["grad_meh_worlds", _maps];
[_loadingDisplay] call (uiNamespace getVariable "grad_meh_fnc_redrawLoading");

// export
{
	gradMehExportMap [
		_x,
		_exportSat,
		_exportHouses,
		_exportPreviewImg,
		_exportMeta,
		_exportDem
	];
} forEach _maps;