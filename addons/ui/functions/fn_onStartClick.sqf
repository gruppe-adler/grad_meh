/*
 * Author: DerZade
 * Triggerd by the "onMouseButtonClick"-EH of the start button
 *
 * Arguments:
 * n/a
 *
 * Return Value:
 * NONE
 *
 * Example:
 * _this call grad_meh_fnc_onStartClick;
 *
 * Public: No
 */
#include "../idcmacros.hpp"

params ["_control", "_button", "_xPos", "_yPos", "_shift", "_ctrl", "_alt"];

private _display = ctrlParent _control;
private _mapList = _display displayCtrl IDC_MAPLIST;

private _mapItems = (allControls _display);
private _maps = [];

{
	private _selected = _x getVariable ["grad_meh_selected", false];

	if (_selected) then {
		_maps pushBack (_x getVariable ["grad_meh_worldName", ""]);
	};
} forEach (allControls _display);

uiNamespace setVariable ["grad_meh_progress", []];
private _loadingDisplay = _display createDisplay "grad_meh_loading";
_loadingDisplay setVariable ["grad_meh_worlds", _maps];
[_loadingDisplay] call (uiNamespace getVariable "grad_meh_fnc_redrawLoading");
