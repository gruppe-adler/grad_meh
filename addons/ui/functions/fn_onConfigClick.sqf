/*
 * Author: DerZade
 * Triggerd by the "onMouseButtonClick"-EH of the config button
 *
 * Arguments:
 * n/a
 *
 * Return Value:
 * NONE
 *
 * Example:
 * _this call grad_meh_fnc_onConfigClick;
 *
 * Public: No
 */
#include "../idcmacros.hpp"

params ["_control", "_button", "_xPos", "_yPos", "_shift", "_ctrl", "_alt"];

private _display = ctrlParent _control;

private _maps = [_display] call (uiNamespace getVariable "grad_meh_fnc_getSelectedMaps");

private _configDisplay = _display createDisplay "grad_meh_config";

_configDisplay setVariable ["grad_meh_selectedMaps", _maps];
