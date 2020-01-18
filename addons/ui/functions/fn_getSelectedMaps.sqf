/*
 * Author: DerZade
 * Get selected maps
 *
 * Arguments:
 * 0: Main display <DISPLAY>
 *
 * Return Value:
 * Selected Maps <ARRAY>
 *
 * Example:
 * [_display] call grad_meh_fnc_getSelectedMaps;
 *
 * Public: No
 */

#include "../idcmacros.hpp"

params ["_display"];

private _mapItems = (allControls _display);
private _maps = [];

{
	private _selected = _x getVariable ["grad_meh_selected", false];

	if (_selected) then {
		_maps pushBack (_x getVariable ["grad_meh_worldName", ""]);
	};
} forEach (allControls _display);

_maps;