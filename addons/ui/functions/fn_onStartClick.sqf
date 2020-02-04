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

private _maps = [_display] call (uiNamespace getVariable "grad_meh_fnc_getSelectedMaps");

[_maps] call (uiNamespace getVariable "grad_meh_fnc_export");
