params ["_display", "_exitCode"];

if !(_exitCode isEqualTo 2) exitWith {};

private _maps = _display getVariable ["grad_meh_worlds", []];

[displayParent _display, _maps] spawn {
	params ["_parent", "_maps"];

	private _result = [
		"Are you sure you want to quit Gruppe Adler MEH?<br/><br/><t size='0.7'>(We recommend restarting your game if you started any export process, to prevent any negative impacts on game performance.)</t>", 
		"Quit Gruppe Adler MEH",
		true,
		true,
		_parent
	] call (uiNamespace getVariable "BIS_fnc_guiMessage");

	if (_result) exitWith {
		// exit mission
		[] spawn {
			sleep 0.5;
			endMission "END1";
		};
	};

	// create loading display
	private _loadingDisplay = _parent createDisplay "grad_meh_loading";
	_loadingDisplay setVariable ["grad_meh_worlds", _maps];
	[_loadingDisplay] call (uiNamespace getVariable "grad_meh_fnc_loading_redraw");
};
