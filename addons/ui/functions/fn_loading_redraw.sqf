/*
 * Author: DerZade
 * Redraw the loading screen
 *
 * Arguments:
 * 0: Loading display <DISPLAY>
 *
 * Return Value:
 * NONE
 *
 * Example:
 * [(uiNamespace getVariable "grad_meh_loadingDisplay")] call grad_meh_fnc_loading_redraw;
 *
 * Public: No
 */

#include "../idcmacros.hpp"

params ["_display"];

private _worlds = _display getVariable ["grad_meh_worlds", []];
private _loadingList = _display displayCtrl IDC_DIALOG_CONTENT;

// clear loadingList 
{
	if (ctrlClassName _x isEqualTo "grad_meh_loadingItem") then {
		ctrlDelete _x;
	};
} forEach (allControls _display);


private _allDone = true;
private _yPos = (SPACING * GRID_H);
{
	private _return = [
		_display,
		_loadingList,
		_x
	] call (uiNamespace getVariable "grad_meh_fnc_loadingItem_create");

	_return params ["_item", "_done"];
	_allDone = _allDone && _done;

	_item ctrlSetPositionY _yPos;
	_item ctrlCommit 0;

	private _height = (ctrlPosition _item) select 3;
	_yPos = _yPos + _height;
} forEach _worlds;

if (_allDone) then {
	[_display, _worlds] spawn {
		params ["_display", "_worlds"];

		private _parent = displayParent _display;
		_display closeDisplay 1;
		private _doneDisp = _parent createDisplay "grad_meh_done";
		_doneDisp setVariable ["grad_meh_worlds", _worlds];
		[_doneDisp] call (uiNamespace getVariable 'grad_meh_fnc_done_onLoad');
	};
};