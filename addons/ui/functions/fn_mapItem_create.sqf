/*
 * Author: DerZade
 * Create map item control with the given parameters
 *
 * Arguments:
 * 0: Display <DISPLAY>
 * 1: Existing controls group to create item in <CONTROL>
 * 2: worldName <STRING>
 * 3: Path to picture of map <STRING>
 * 4: Displayname <STRING>
 * 5: Author <STRING>
 * 6: Selected <BOOLEAN>
 *
 * Return Value:
 * NONE
 *
 * Example:
 * _this call grad_meh_fnc_createMapItem;
 *
 * Public: No
 */
#include "../idcmacros.hpp"

params ["_display", "_parentGrp", "_worldName", "_image", "_displayName", "_author", "_selected"];

private _grp = _display ctrlCreate ['grad_meh_mapItem', -1, _parentGrp];
(_grp controlsGroupCtrl IDC_MAPITEM_PICTURE) ctrlSetText _image;
(_grp controlsGroupCtrl IDC_MAPITEM_NAME) ctrlSetText _displayName;
(_grp controlsGroupCtrl IDC_MAPITEM_AUTHOR) ctrlSetText format [" - %1", _author];
_grp setVariable ["grad_meh_worldName", _worldName];

if (_selected) then { (_grp controlsGroupCtrl IDC_MAPITEM_NAME) call (uiNamespace getVariable "grad_meh_fnc_mapItem_onClick"); };

_grp;