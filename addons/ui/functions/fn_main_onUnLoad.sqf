params ["_display", "_exitCode"];

// get selected maps
private _maps = [];
{
	private _selected = _x getVariable ["grad_meh_selected", false];

	if (_selected) then {
		_maps pushBack (_x getVariable ["grad_meh_worldName", ""]);
	};
} forEach (allControls _display);

// save in ui namespace
uiNamespace setVariable ["grad_meh_selectedMaps", _maps];

if (_exitCode isEqualTo 1) then {
	// user pressed ok
	if (count _maps isEqualTo 0) then {
		(displayParent _display) spawn { _this createDisplay "grad_meh_main"; };
	} else {
		(displayParent _display) spawn { _this createDisplay "grad_meh_config"; };
	}
} else {
	// user pressed cancel
	[displayParent _display] spawn {
		params ["_parent"];

		private _result = [
			"Are you sure you want to quit Gruppe Adler MEH?", 
			"Quit Gruppe Adler MEH",
			true,
			true,
			_parent
		] call (uiNamespace getVariable "BIS_fnc_guiMessage");

		if (_result) exitWith {};

		// create loading display
		private _loadingDisplay = _parent createDisplay "grad_meh_main";
	};
}
