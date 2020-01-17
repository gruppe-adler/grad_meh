/*
 * Author: DerZade
 * Triggerd by the "onKeyDown"-EH of the main display
 *
 * Arguments:
 * n/a
 *
 * Return Value:
 * NONE
 *
 * Example:
 * _this call grad_meh_fnc_onKeyDown;
 *
 * Public: No
 */

params ["_display", "_key", "_shift", "_ctrl", "_alt"];

if (_key isEqualTo 1) exitWith { // ESC
	[_display] spawn {
		private _result = [
			"Do you want to quit grad_meh? <br/><br/><t size='0.7'>(We recommend restarting your game if you started any export process, to prevent any negative impacts on game performance.)</t>", 
			"Quit grad_meh",
			true,
			true,
			(_this select 0)
		] call (uiNamespace getVariable "BIS_fnc_guiMessage");

		if (!_result) exitWith {};

		(_this select 0) closeDisplay 1;
	};
	true;
};
