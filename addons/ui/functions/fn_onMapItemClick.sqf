/*
 * Author: DerZade
 * Triggerd by the 'onMouseDown'-EH of any map item control
 *
 * Arguments:
 * n/a
 *
 * Return Value:
 * NONE
 *
 * Example:
 * _this call grad_meh_fnc_onMapItemClick;
 *
 * Public: No
 */
#include "../idcmacros.hpp"

params ["_control", "_button", "_xPos", "_yPos", "_shift", "_ctrl", "_alt"];

private _grp = ctrlParentControlsGroup _control;
private _selected = _grp getVariable ['grad_meh_selected', false];
_grp setVariable ['grad_meh_selected', !_selected];
(_grp controlsGroupCtrl IDC_MAPITEM_SELECTINDICATOR) ctrlShow (!_selected);